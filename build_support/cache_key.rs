use crate::build_env as env;
use sha2::{Digest, Sha256};
#[cfg(unix)]
use std::{ffi::OsStr, process::Command};
use std::{
    fs, io,
    path::{Path, PathBuf},
};

fn hex(bytes: &[u8]) -> String {
    const DIGITS: &[u8; 16] = b"0123456789abcdef";
    bytes
        .iter()
        .flat_map(|byte| {
            [
                char::from(DIGITS[usize::from(byte >> 4)]),
                char::from(DIGITS[usize::from(byte & 15)]),
            ]
        })
        .collect()
}

#[cfg(unix)]
fn field(hash: &mut Sha256, bytes: &[u8]) {
    hash.update((bytes.len() as u64).to_le_bytes());
    hash.update(bytes);
}

pub(super) fn digest_file(path: &Path) -> io::Result<String> {
    let mut file = fs::File::open(path)?;
    let mut hash = Sha256::new();
    let mut buffer = [0_u8; 8192];
    loop {
        let count = io::Read::read(&mut file, &mut buffer)?;
        if count == 0 {
            break;
        }
        hash.update(&buffer[..count]);
    }
    Ok(hex(&hash.finalize()))
}

pub(super) fn files(root: &Path) -> io::Result<Vec<PathBuf>> {
    fn visit(root: &Path, dir: &Path, paths: &mut Vec<PathBuf>) -> io::Result<()> {
        for entry in fs::read_dir(dir)? {
            let entry = entry?;
            let kind = entry.file_type()?;
            if kind.is_dir() {
                visit(root, &entry.path(), paths)?;
            } else if kind.is_file() {
                paths.push(entry.path().strip_prefix(root).unwrap().to_path_buf());
            } else {
                return Err(io::Error::other(format!(
                    "native cache refuses non-regular file: {}",
                    entry.path().display()
                )));
            }
        }
        Ok(())
    }
    let mut paths = Vec::new();
    visit(root, root, &mut paths)?;
    paths.sort();
    Ok(paths)
}

#[cfg(unix)]
fn executable(path: &Path) -> io::Result<PathBuf> {
    if path.components().count() > 1 {
        return path.canonicalize();
    }
    std::env::split_paths(&env::var_os("PATH").unwrap_or_default())
        .map(|dir| dir.join(path))
        .find(|candidate| candidate.is_file())
        .ok_or_else(|| {
            io::Error::new(
                io::ErrorKind::NotFound,
                format!("native cache cannot identify {}", path.display()),
            )
        })?
        .canonicalize()
}

#[cfg(unix)]
fn command_identity(hash: &mut Sha256, command: &Command) -> io::Result<PathBuf> {
    let path = executable(Path::new(command.get_program()))?;
    field(hash, path.as_os_str().as_encoded_bytes());
    field(hash, digest_file(&path)?.as_bytes());
    for arg in command.get_args() {
        field(hash, arg.as_encoded_bytes());
    }
    for (name, value) in command.get_envs() {
        field(hash, name.as_encoded_bytes());
        field(hash, value.unwrap_or(OsStr::new("")).as_encoded_bytes());
    }
    Ok(path)
}

#[cfg(unix)]
fn tool(hash: &mut Sha256, command: &mut Command) -> io::Result<()> {
    let path = command_identity(hash, command)?;
    let output = command.arg("--version").output()?;
    if !output.status.success() {
        return Err(io::Error::other(format!(
            "native cache could not identify {}: --version failed",
            path.display()
        )));
    }
    field(hash, &output.stdout);
    field(hash, &output.stderr);
    Ok(())
}

#[cfg(unix)]
fn target_env(base: &str, target: &str, host: &str) -> Option<std::ffi::OsString> {
    [
        format!("{base}_{target}"),
        format!("{base}_{}", target.replace('-', "_")),
        format!("{}_{base}", if target == host { "HOST" } else { "TARGET" }),
        base.to_owned(),
    ]
    .iter()
    .find_map(env::var_os)
}

/// Length-delimited SHA-256 of source bytes and all inputs to this native build.
/// `OUT_DIR`, job counts and the cache location do not change the native output.
#[cfg(unix)]
pub(super) fn key(root: &Path) -> io::Result<String> {
    let mut hash = Sha256::new();
    field(&mut hash, b"lbug-native-cache-v1");
    field(&mut hash, include_bytes!("../build.rs"));
    field(&mut hash, include_bytes!("native_cache.rs"));
    field(&mut hash, include_bytes!("cache_key.rs"));
    field(&mut hash, include_bytes!("build_env.rs"));
    field(&mut hash, include_bytes!("../Cargo.lock"));
    for path in files(root)? {
        field(&mut hash, path.as_os_str().as_encoded_bytes());
        field(&mut hash, digest_file(&root.join(path))?.as_bytes());
    }
    println!("cargo:rerun-if-changed={}", root.display());
    let target = env::var("TARGET").map_err(io::Error::other)?;
    let host = env::var("HOST").map_err(io::Error::other)?;
    for name in env::native_names() {
        field(&mut hash, name.as_bytes());
        // Preserve absent versus empty (e.g. presence of LBUG_SHARED).
        if let Some(value) = env::var_os(&name) {
            field(&mut hash, b"set");
            field(&mut hash, value.as_encoded_bytes());
        } else {
            field(&mut hash, b"unset");
        }
    }
    for cpp in [false, true] {
        let mut build = cc::Build::new();
        build
            .cpp(cpp)
            .cargo_metadata(false)
            .opt_level(0)
            .debug(false)
            .warnings(false)
            .host(&host)
            .target(&target);
        let compiler = build.try_get_compiler().map_err(io::Error::other)?;
        let mut direct = Command::new(compiler.path());
        direct
            .args(compiler.args())
            .envs(compiler.env().iter().cloned());
        tool(&mut hash, &mut direct)?;
        if !cpp {
            command_identity(
                &mut hash,
                &build.try_get_archiver().map_err(io::Error::other)?,
            )?;
            command_identity(
                &mut hash,
                &build.try_get_ranlib().map_err(io::Error::other)?,
            )?;
        }
        // cc may wrap the compiler in sccache/ccache; identify both executables.
        let mut wrapped = compiler.to_command();
        if wrapped.get_program() != compiler.path() {
            tool(&mut hash, &mut wrapped)?;
        }
    }
    tool(
        &mut hash,
        &mut Command::new(env::var_os("RUSTC").unwrap_or_else(|| "rustc".into())),
    )?;
    tool(
        &mut hash,
        &mut Command::new(target_env("CMAKE", &target, &host).unwrap_or_else(|| "cmake".into())),
    )?;
    Ok(hex(&hash.finalize()))
}

/// Files loaded outside the source tree cannot be bounded by this key. Fail
/// closed instead of treating an arbitrary CMake/response file as a flag value.
pub(super) fn bypass() -> Option<String> {
    let mut first = None;
    for name in env::bypass_names() {
        let present = env::var_os(&name).is_some();
        println!(
            "native-cache environment: {name}: {}",
            if present { "set" } else { "unset" }
        );
        if present && first.is_none() {
            first = Some(name);
        }
    }
    if first.is_some() {
        return first;
    }
    for name in env::native_names() {
        let Some(value) = env::var_os(&name) else {
            continue;
        };
        let value = value.to_string_lossy();
        if env::is_flag(&name)
            && value.split_whitespace().any(|flag| {
                flag.starts_with('@')
                    || [
                        "-include",
                        "-imacros",
                        "-I",
                        "-isystem",
                        "-iquote",
                        "--sysroot",
                        "-isysroot",
                        "-fplugin",
                        "-specs",
                        "--specs",
                        "--config",
                        "-ivfsoverlay",
                        "-fprofile-use",
                        "-Xclang",
                    ]
                    .iter()
                    .any(|prefix| flag.starts_with(prefix))
            })
        {
            return Some(name);
        }
        if env::is_command(&name)
            && value.contains(char::is_whitespace)
            && !Path::new(value.as_ref()).is_file()
        {
            return Some(name);
        }
    }
    None
}
