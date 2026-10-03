use crate::build_env as env;
#[cfg(all(test, unix))]
#[path = "../tests/native-cache/cache_rustflags.rs"]
mod rustflags_tests;
#[cfg(unix)]
use sha2::{Digest, Sha256};
use std::path::Path;
#[cfg(unix)]
use std::{ffi::OsStr, fs, io, path::PathBuf, process::Command};

#[cfg(unix)]
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

#[cfg(unix)]
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

#[cfg(unix)]
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

// cargo-llvm-cov 0.9.1 src/main.rs:155–159,194–198,260–264;
// cargo-semver-checks 0.50.0 src/data_generation/generate.rs:36–40.
// cc 1.4.2 / 1.5.1 do not inherit these into native compiler flags.
#[cfg(unix)]
const RUST_ONLY_CFGS: &[(&str, &str)] = &[
    ("coverage", "CARGO_CFG_COVERAGE"),
    ("coverage_nightly", "CARGO_CFG_COVERAGE_NIGHTLY"),
    ("trybuild_no_target", "CARGO_CFG_TRYBUILD_NO_TARGET"),
];

#[cfg(unix)]
const RUST_ONLY_FLAGS: &[&[&str]] = &[
    &["-C", "instrument-coverage"],
    &["-Cinstrument-coverage"],
    &["--cap-lints", "allow"],
    &["--cap-lints=allow"],
];

#[cfg(unix)]
fn native_rustflags(name: &str, value: &OsStr) -> std::ffi::OsString {
    let Some(value) = value.to_str() else {
        return value.to_owned();
    };
    let tokens: Vec<_> = if name == "CARGO_ENCODED_RUSTFLAGS" {
        value.split('\u{1f}').collect()
    } else {
        // Cargo splits RUSTFLAGS on whitespace, not shell quoting.
        value.split_whitespace().collect()
    };
    let mut kept = Vec::new();
    let mut index = 0;
    while let Some(&flag) = tokens.get(index) {
        let next = tokens.get(index + 1).copied();
        if let Some(spelling) = RUST_ONLY_FLAGS
            .iter()
            .find(|spelling| tokens[index..].starts_with(spelling))
        {
            index += spelling.len();
            continue;
        }
        if flag == "--cfg"
            && next.is_some_and(|cfg| RUST_ONLY_CFGS.iter().any(|(bare, _)| *bare == cfg))
        {
            index += 2;
            continue;
        }
        if flag
            .strip_prefix("--cfg=")
            .is_some_and(|cfg| RUST_ONLY_CFGS.iter().any(|(bare, _)| *bare == cfg))
        {
            index += 1;
            continue;
        }
        kept.push(flag);
        index += 1;
        // Known joined options have no following argument. All other switches
        // retain the next token as opaque, even if it resembles an allowlist
        // entry. Over-retention costs a miss rather than an incorrect hit.
        let joined = ["-C", "-Z", "-L", "-l", "-W", "-A", "-D", "-F"]
            .iter()
            .any(|prefix| flag.starts_with(prefix) && flag.len() > prefix.len())
            || ["--cfg=", "--cap-lints=", "--codegen="]
                .iter()
                .any(|prefix| flag.starts_with(prefix));
        if flag.starts_with('-') && !joined {
            if let Some(next) = next {
                kept.push(next);
                index += 1;
            }
        }
    }
    kept.join(if name == "CARGO_ENCODED_RUSTFLAGS" {
        "\u{1f}"
    } else {
        " "
    })
    .into()
}

#[cfg(unix)]
fn native_value(name: &str) -> Option<std::ffi::OsString> {
    let value = env::var_os(name)?;
    if RUST_ONLY_CFGS.iter().any(|(_, variable)| *variable == name) && value.is_empty() {
        return None;
    }
    if name == "RUSTFLAGS" || name == "CARGO_ENCODED_RUSTFLAGS" {
        let value = native_rustflags(name, &value);
        return if value.is_empty() { None } else { Some(value) };
    }
    Some(value)
}

/// Length-delimited SHA-256 of source bytes and all inputs to this native build.
/// `OUT_DIR`, job counts and the cache location do not change the native output.
#[cfg(unix)]
pub(super) fn key(root: &Path) -> io::Result<String> {
    let mut hash = Sha256::new();
    field(&mut hash, b"lbug-native-cache-v1");
    field(&mut hash, include_bytes!("../build.rs"));
    field(&mut hash, include_bytes!("native_cache.rs"));
    field(&mut hash, include_bytes!("cache_dir.rs"));
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
        let value = native_value(&name);
        // Cargo derives empty variables for the three bare Rust-only cfgs.
        // Omit their names too: absent dynamic cfgs are not in native_names.
        if value.is_none() && RUST_ONLY_CFGS.iter().any(|(_, variable)| *variable == name) {
            continue;
        }
        field(&mut hash, name.as_bytes());
        // Preserve absent versus empty except for the proven Rust-only inputs.
        if let Some(value) = value {
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

fn file_loading_flags(value: &str) -> bool {
    // Match cc::Build::envflags and get_env_boolean, including ASCII-only
    // whitespace when shell escaping is off. No second, approximate parser.
    let shell = env::var_os("CC_SHELL_ESCAPED_FLAGS").is_some_and(|value| {
        !value.is_empty() && value != "0" && value != "no" && value != "false"
    });
    if shell {
        let mut tokens = shlex::Shlex::new(value);
        let loads = tokens.any(|flag| file_loading_flag(&flag));
        loads || tokens.had_error
    } else {
        value.split_ascii_whitespace().any(file_loading_flag)
    }
}

enum ScalarRule {
    Macro,
    Optimization,
    Debug,
    Standard,
    Warning,
    Exact,
}

// Only bounded scalar families may be keyed. Unknown switches, positional
// files, response files and path-bearing options bypass instead of guessing.
// These include the fixture and ordinary native hardening/PIC/ABI flags;
// Rust flags are keyed separately, minus the proven Rust-only allowlist.
const SCALAR_FLAGS: &[(&str, ScalarRule)] = &[
    ("-D", ScalarRule::Macro),
    ("-U", ScalarRule::Macro),
    ("-O", ScalarRule::Optimization),
    ("-g", ScalarRule::Debug),
    ("-std=", ScalarRule::Standard),
    ("-W", ScalarRule::Warning),
    ("-fPIC", ScalarRule::Exact),
    ("-fpic", ScalarRule::Exact),
    ("-fPIE", ScalarRule::Exact),
    ("-fpie", ScalarRule::Exact),
    ("-fexceptions", ScalarRule::Exact),
    ("-fno-exceptions", ScalarRule::Exact),
    ("-frtti", ScalarRule::Exact),
    ("-fno-rtti", ScalarRule::Exact),
    ("-fomit-frame-pointer", ScalarRule::Exact),
    ("-fno-omit-frame-pointer", ScalarRule::Exact),
    ("-fstack-protector", ScalarRule::Exact),
    ("-fstack-protector-strong", ScalarRule::Exact),
    ("-fstack-protector-all", ScalarRule::Exact),
    ("-fstrict-aliasing", ScalarRule::Exact),
    ("-fno-strict-aliasing", ScalarRule::Exact),
    ("-fvisibility=hidden", ScalarRule::Exact),
    ("-m32", ScalarRule::Exact),
    ("-m64", ScalarRule::Exact),
    ("-pthread", ScalarRule::Exact),
];

fn file_loading_flag(flag: &str) -> bool {
    !SCALAR_FLAGS.iter().any(|(prefix, rule)| {
        let Some(suffix) = flag.strip_prefix(prefix) else {
            return false;
        };
        match rule {
            ScalarRule::Macro => !suffix.is_empty() && !suffix.contains(['/', '\\', '@']),
            ScalarRule::Optimization => {
                ["", "0", "1", "2", "3", "s", "z", "g", "fast"].contains(&suffix)
            }
            ScalarRule::Debug => [
                "",
                "0",
                "1",
                "2",
                "3",
                "line-tables-only",
                "dwarf-2",
                "dwarf-3",
                "dwarf-4",
                "dwarf-5",
            ]
            .contains(&suffix),
            ScalarRule::Standard => {
                !suffix.is_empty()
                    && suffix
                        .chars()
                        .all(|c| c.is_ascii_alphanumeric() || c == '+' || c == '-')
            }
            ScalarRule::Warning => {
                !suffix.is_empty()
                    && suffix
                        .chars()
                        .all(|c| c.is_ascii_alphanumeric() || c == '-' || c == '=')
            }
            ScalarRule::Exact => suffix.is_empty(),
        }
    })
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
        if env::is_flag(&name) && file_loading_flags(&value) {
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
