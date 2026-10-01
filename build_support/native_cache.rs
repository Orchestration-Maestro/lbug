use crate::build_env as env;
#[cfg(unix)]
#[path = "cache_dir.rs"]
mod cache_dir;
#[path = "cache_key.rs"]
mod cache_key;

#[cfg(unix)]
use cache_dir::Directory;
#[cfg(unix)]
use std::{fmt::Write, fs, io::Read, os::unix::fs::MetadataExt};
use std::{
    io,
    path::{Path, PathBuf},
};

#[cfg(unix)]
pub(crate) struct NativeCache {
    root_path: PathBuf,
    root: Directory,
    entry: String,
    key: String,
}

// Ownership verification is unavailable: no cache can be constructed here.
#[cfg(not(unix))]
pub(crate) enum NativeCache {}

impl NativeCache {
    pub(crate) fn from_env(source: &Path) -> io::Result<Option<Self>> {
        let Some(root) = env::var_os("LBUG_NATIVE_CACHE_DIR") else {
            return Ok(None);
        };
        if let Some(variable) = cache_key::bypass() {
            println!("cargo:warning=native cache disabled by {variable}: external toolchain/file inputs cannot be fully identified; building from source");
            return Ok(None);
        }
        std::fs::metadata(source)?;
        #[cfg(not(unix))]
        {
            let _ = root;
            println!("cargo:warning=LBUG_NATIVE_CACHE_DIR disabled: cache ownership cannot be verified on this platform; building from source");
            Ok(None)
        }
        #[cfg(unix)]
        {
            use std::os::unix::fs::DirBuilderExt;
            let root_path = PathBuf::from(root);
            fs::DirBuilder::new()
                .recursive(true)
                .mode(0o700)
                .create(&root_path)?;
            let root = Directory::open(&root_path).map_err(|error| {
                io::Error::other(format!(
                    "untrusted LBUG_NATIVE_CACHE_DIR {}: {error}",
                    root_path.display()
                ))
            })?;
            let key = cache_key::key(source)?;
            let entry = format!("entry-{key}");
            Ok(Some(Self {
                root_path,
                root,
                entry,
                key,
            }))
        }
    }

    #[cfg(not(unix))]
    pub(crate) fn get_or_build(self, _build: impl FnOnce(&Path) -> PathBuf) -> io::Result<PathBuf> {
        match self {}
    }
}

#[cfg(unix)]
impl NativeCache {
    fn unchanged_root(&self) -> io::Result<()> {
        let current = Directory::open(&self.root_path)?;
        let held = cache_dir::trusted(&self.root.0, true)?;
        let current = current.0.metadata()?;
        if held.dev() != current.dev() || held.ino() != current.ino() {
            return Err(io::Error::other("LBUG_NATIVE_CACHE_DIR was replaced"));
        }
        Ok(())
    }

    // Verify the bytes already copied from no-follow, checked handles. The linker
    // only sees target-owned copies, never a replaceable cache pathname.
    fn verified_copy(&self, destination: &Path) -> io::Result<()> {
        let entry = self.root.directory(self.entry.as_ref())?;
        let mut manifest = entry.open_file("manifest.sha256".as_ref())?;
        cache_dir::trusted(&manifest, false)?;
        let mut text = String::new();
        manifest.read_to_string(&mut text)?;
        let mut lines = text.lines();
        if lines.next() != Some(format!("lbug-native-cache-v1 {}", self.key).as_str()) {
            return Err(io::Error::other("native cache manifest key mismatch"));
        }
        let mut actual = Vec::new();
        copy_checked(&entry, destination, Path::new(""), &mut actual)?;
        actual.sort();
        if actual.is_empty() || actual.iter().map(String::as_str).ne(lines) {
            return Err(io::Error::other(
                "native cache artifact digest/inventory mismatch",
            ));
        }
        Ok(())
    }

    pub(crate) fn get_or_build(self, build: impl FnOnce(&Path) -> PathBuf) -> io::Result<PathBuf> {
        self.run(
            build,
            |root, source, destination| root.publish(source.as_os_str(), destination.as_os_str()),
            fs::File::sync_all,
        )
    }

    #[cfg(test)]
    pub(crate) fn get_or_build_with(
        self,
        build: impl FnOnce(&Path) -> PathBuf,
        publish: impl FnOnce(&Path, &Path) -> io::Result<()>,
    ) -> io::Result<PathBuf> {
        self.run(
            build,
            |_, source, destination| publish(source, destination),
            fs::File::sync_all,
        )
    }

    #[cfg(test)]
    pub(crate) fn get_or_build_with_sync(
        self,
        build: impl FnOnce(&Path) -> PathBuf,
        sync: impl Fn(&fs::File) -> io::Result<()>,
    ) -> io::Result<PathBuf> {
        self.run(
            build,
            |root, source, destination| root.publish(source.as_os_str(), destination.as_os_str()),
            sync,
        )
    }

    fn run(
        self,
        build: impl FnOnce(&Path) -> PathBuf,
        publish: impl FnOnce(&Directory, &Path, &Path) -> io::Result<()>,
        sync: impl Fn(&fs::File) -> io::Result<()>,
    ) -> io::Result<PathBuf> {
        self.unchanged_root()?;
        let out = PathBuf::from(
            env::var_os("OUT_DIR").ok_or_else(|| io::Error::other("missing OUT_DIR"))?,
        );
        fs::create_dir_all(&out)?;
        let snapshot = tempfile::Builder::new()
            .prefix("native-engine-")
            .tempdir_in(&out)?;
        if self.verified_copy(snapshot.path()).is_ok() {
            println!(
                "cargo:warning=native cache hit: {}",
                self.root_path.join(&self.entry).display()
            );
            return Ok(snapshot.keep());
        }
        // Discard all bytes from a refused entry before staging a fresh build.
        snapshot.close()?;
        println!(
            "cargo:warning=native cache miss/refusal: {}",
            self.root_path.join(&self.entry).display()
        );
        let work = tempfile::Builder::new().prefix(".work-").tempdir_in(&out)?;
        let built = build(work.path());
        let payload = tempfile::Builder::new()
            .prefix("native-engine-")
            .tempdir_in(&out)?;
        let mut inventory = Vec::new();
        collect_artifacts(
            &built.join("build/src"),
            &built,
            payload.path(),
            &mut inventory,
        )?;
        if !inventory.iter().any(|path| {
            path.file_name().is_some_and(|name| {
                name.to_string_lossy().starts_with("liblbug")
                    || name.to_string_lossy().starts_with("lbug.")
            })
        }) {
            return Err(io::Error::other(
                "native cache build produced no lbug library",
            ));
        }
        inventory.sort();
        let mut text = format!("lbug-native-cache-v1 {}\n", self.key);
        for path in inventory {
            writeln!(
                text,
                "{}  {}",
                cache_key::digest_file(&payload.path().join(&path))?,
                path.to_string_lossy().replace('\\', "/")
            )
            .expect("writing to String is infallible");
        }
        fs::write(payload.path().join("manifest.sha256"), text)?;
        self.unchanged_root()?;
        let private_name = payload.path().file_name().unwrap();
        let private = self.root.create_directory(private_name)?;
        let publication = (|| {
            stage_synced(payload.path(), &private, &sync)?;
            publish(&self.root, Path::new(private_name), Path::new(&self.entry))
        })();
        match publication {
            Ok(()) => {
                self.root.0.sync_all()?;
                println!(
                    "cargo:warning=native cache published: {}",
                    self.root_path.join(&self.entry).display()
                );
            }
            Err(error) => {
                self.root.remove_tree(private_name)?;
                println!(
                    "cargo:warning=native cache not published ({error}); keeping private output"
                );
            }
        }
        Ok(payload.keep())
    }
}

#[cfg(unix)]
fn copy_checked(
    dir: &Directory,
    destination: &Path,
    relative: &Path,
    inventory: &mut Vec<String>,
) -> io::Result<()> {
    fs::create_dir_all(destination)?;
    for name in dir.names()? {
        if relative.as_os_str().is_empty() && name == "manifest.sha256" {
            continue;
        }
        let mut file = dir.open_file(&name)?;
        let directory = file.metadata()?.is_dir();
        cache_dir::trusted(&file, directory)?;
        let path = relative.join(&name);
        if directory {
            copy_checked(&Directory(file), &destination.join(&name), &path, inventory)?;
        } else {
            let target = destination.join(&name);
            let mut output = fs::OpenOptions::new()
                .write(true)
                .create_new(true)
                .open(&target)?;
            io::copy(&mut file, &mut output)?;
            inventory.push(format!(
                "{}  {}",
                cache_key::digest_file(&target)?,
                path.to_string_lossy().replace('\\', "/")
            ));
        }
    }
    Ok(())
}

#[cfg(unix)]
fn stage_synced(
    source: &Path,
    destination: &Directory,
    sync: &impl Fn(&fs::File) -> io::Result<()>,
) -> io::Result<()> {
    for item in fs::read_dir(source)? {
        let item = item?;
        if item.file_type()?.is_dir() {
            let dir = destination.create_directory(&item.file_name())?;
            stage_synced(&item.path(), &dir, sync)?;
        } else {
            let mut input = fs::File::open(item.path())?;
            let mut output = destination.create_file(&item.file_name())?;
            io::copy(&mut input, &mut output)?;
            sync(&output)?;
        }
    }
    destination.0.sync_all()
}

#[cfg(unix)]
fn collect_artifacts(
    dir: &Path,
    built: &Path,
    payload: &Path,
    inventory: &mut Vec<PathBuf>,
) -> io::Result<()> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();
        if entry.file_type()?.is_dir() {
            collect_artifacts(&path, built, payload, inventory)?;
        } else {
            let name = entry.file_name();
            let name = name.to_string_lossy();
            if !(name.ends_with(".a")
                || name.ends_with(".lib")
                || name.ends_with(".dll")
                || name.contains(".so")
                || name.contains(".dylib")
                || name.ends_with(".h")
                || name.ends_with(".hpp"))
            {
                continue;
            }
            let relative = path.strip_prefix(built).unwrap().to_path_buf();
            let destination = payload.join(&relative);
            fs::create_dir_all(destination.parent().unwrap())?;
            fs::copy(path, &destination)?;
            inventory.push(relative);
        }
    }
    Ok(())
}
