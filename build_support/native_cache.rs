use crate::build_env as env;
#[path = "cache_key.rs"]
mod cache_key;

use std::{
    fmt::Write,
    fs, io,
    path::{Path, PathBuf},
};

pub(crate) struct NativeCache {
    root: PathBuf,
    entry: PathBuf,
    key: String,
}

impl NativeCache {
    pub(crate) fn from_env(source: &Path) -> io::Result<Option<Self>> {
        let Some(root) = env::var_os("LBUG_NATIVE_CACHE_DIR") else {
            return Ok(None);
        };
        if let Some(variable) = cache_key::bypass() {
            println!("cargo:warning=native cache disabled by {variable}: external toolchain/file inputs cannot be fully identified; building from source");
            return Ok(None);
        }
        // The selected source must be accessible even on source-only platforms.
        fs::metadata(source)?;
        #[cfg(not(unix))]
        {
            let _ = root;
            println!("cargo:warning=LBUG_NATIVE_CACHE_DIR disabled: cache ownership cannot be verified on this platform; building from source");
            Ok(None)
        }
        #[cfg(unix)]
        {
            use std::os::unix::fs::DirBuilderExt;
            let root = PathBuf::from(root);
            fs::DirBuilder::new()
                .recursive(true)
                .mode(0o700)
                .create(&root)?;
            trusted(&root, true).map_err(|error| {
                io::Error::other(format!(
                    "untrusted LBUG_NATIVE_CACHE_DIR {}: {error}",
                    root.display()
                ))
            })?;
            let root = root.canonicalize()?;
            let key = cache_key::key(source)?;
            let entry = root.join(format!("entry-{key}"));
            Ok(Some(Self { root, entry, key }))
        }
    }

    fn valid(&self) -> io::Result<()> {
        trusted(&self.entry, true)?;
        let manifest = self.entry.join("manifest.sha256");
        trusted(&manifest, false)?;
        let text = fs::read_to_string(&manifest)?;
        let mut lines = text.lines();
        if lines.next() != Some(format!("lbug-native-cache-v1 {}", self.key).as_str()) {
            return Err(io::Error::other("native cache manifest key mismatch"));
        }
        let paths = cache_key::files(&self.entry)?;
        let mut actual = Vec::new();
        for path in paths {
            if path == Path::new("manifest.sha256") {
                continue;
            }
            let full = self.entry.join(&path);
            for parent in path
                .ancestors()
                .skip(1)
                .filter(|path| !path.as_os_str().is_empty())
            {
                trusted(&self.entry.join(parent), true)?;
            }
            trusted(&full, false)?;
            actual.push(format!(
                "{}  {}",
                cache_key::digest_file(&full)?,
                path.to_string_lossy().replace('\\', "/")
            ));
        }
        // Checking the exact inventory refuses omitted, extra, duplicate and
        // traversing manifest names without ever opening a manifest-supplied path.
        if actual.is_empty() || actual.iter().map(String::as_str).ne(lines) {
            return Err(io::Error::other(
                "native cache artifact digest/inventory mismatch",
            ));
        }
        Ok(())
    }

    pub(crate) fn get_or_build(self, build: impl FnOnce(&Path) -> PathBuf) -> io::Result<PathBuf> {
        self.get_or_build_with(build, rename_no_replace)
    }

    pub(crate) fn get_or_build_with(
        self,
        build: impl FnOnce(&Path) -> PathBuf,
        publish: impl FnOnce(&Path, &Path) -> io::Result<()>,
    ) -> io::Result<PathBuf> {
        if self.valid().is_ok() {
            println!("cargo:warning=native cache hit: {}", self.entry.display());
            return Ok(self.entry);
        }
        println!(
            "cargo:warning=native cache miss/refusal: {}",
            self.entry.display()
        );
        let work = tempfile::Builder::new()
            .prefix(".work-")
            .tempdir_in(&self.root)?;
        let built = build(work.path());
        let payload = tempfile::Builder::new()
            .prefix(".private-")
            .tempdir_in(&self.root)?;
        let mut inventory = Vec::new();
        // Store only runtime/link artifacts and generated headers, not object
        // files or CMake state tied to the private build/source location.
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
        let manifest = payload.path().join("manifest.sha256");
        fs::write(&manifest, text)?;
        private_permissions(&manifest, false)?;
        sync_tree(payload.path())?;
        match publish(payload.path(), &self.entry) {
            Ok(()) => {
                fs::File::open(&self.root)?.sync_all()?;
                println!(
                    "cargo:warning=native cache published: {}",
                    self.entry.display()
                );
                Ok(self.entry)
            }
            Err(error) => {
                // Another writer may have won; never replace even an empty or
                // invalid entry. Unsupported no-replace also builds privately.
                if self.valid().is_ok() {
                    return Ok(self.entry);
                }
                println!(
                    "cargo:warning=native cache not published ({error}); keeping private output"
                );
                Ok(payload.keep())
            }
        }
    }
}

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
            // Dereference CMake's shared-library aliases into independent files.
            fs::copy(path, &destination)?;
            private_permissions(&destination, false)?;
            inventory.push(relative);
        }
    }
    Ok(())
}

fn sync_tree(dir: &Path) -> io::Result<()> {
    private_permissions(dir, true)?;
    for entry in fs::read_dir(dir)? {
        let path = entry?.path();
        if path.is_dir() {
            sync_tree(&path)?;
        } else {
            fs::File::open(path)?.sync_all()?;
        }
    }
    fs::File::open(dir)?.sync_all()
}

fn private_permissions(path: &Path, directory: bool) -> io::Result<()> {
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        fs::set_permissions(
            path,
            fs::Permissions::from_mode(if directory { 0o700 } else { 0o600 }),
        )
    }
    #[cfg(not(unix))]
    {
        let _ = (path, directory);
        Err(io::Error::new(
            io::ErrorKind::Unsupported,
            "cache permissions cannot be verified on this platform",
        ))
    }
}

fn trusted(path: &Path, directory: bool) -> io::Result<()> {
    #[cfg(unix)]
    {
        use std::os::unix::fs::MetadataExt;
        let metadata = fs::symlink_metadata(path)?;
        // SAFETY: geteuid takes no arguments and has no memory preconditions.
        let uid = unsafe { libc::geteuid() };
        if metadata.uid() != uid
            || metadata.mode() & 0o022 != 0
            || (directory && !metadata.is_dir())
            || (!directory && (!metadata.is_file() || metadata.nlink() != 1))
        {
            return Err(io::Error::other("expected current-user-owned, non-group/other-writable regular cache entry (no links)"));
        }
        Ok(())
    }
    #[cfg(not(unix))]
    {
        let _ = (path, directory);
        Err(io::Error::other(
            "native cache ownership verification unavailable",
        ))
    }
}

/// Atomically publish without replacing any existing name. Never fall back to
/// plain rename: it can replace an empty directory planted by another writer.
fn rename_no_replace(source: &Path, destination: &Path) -> io::Result<()> {
    #[cfg(any(target_os = "linux", target_os = "macos"))]
    {
        use std::{ffi::CString, os::unix::ffi::OsStrExt};
        let source = CString::new(source.as_os_str().as_bytes())?;
        let destination = CString::new(destination.as_os_str().as_bytes())?;
        // SAFETY: both C strings are valid for the duration of this call.
        #[cfg(target_os = "linux")]
        let result = unsafe {
            libc::renameat2(
                libc::AT_FDCWD,
                source.as_ptr(),
                libc::AT_FDCWD,
                destination.as_ptr(),
                libc::RENAME_NOREPLACE,
            )
        };
        // SAFETY: both C strings are valid for the duration of this call.
        #[cfg(target_os = "macos")]
        let result =
            unsafe { libc::renamex_np(source.as_ptr(), destination.as_ptr(), libc::RENAME_EXCL) };
        if result == 0 {
            Ok(())
        } else {
            Err(io::Error::last_os_error())
        }
    }
    #[cfg(not(any(target_os = "linux", target_os = "macos")))]
    {
        let _ = (source, destination);
        Err(io::Error::new(
            io::ErrorKind::Unsupported,
            "atomic no-replace rename unsupported",
        ))
    }
}
