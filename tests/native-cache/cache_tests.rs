use std::{env, fs, path::PathBuf, process::Command};
use tempfile::TempDir;
#[cfg(unix)]
#[path = "cache_flags.rs"]
mod flags;
#[cfg(unix)]
#[path = "cache_guards.rs"]
mod guards;
#[cfg(unix)]
#[path = "cache_handles.rs"]
mod handles;
#[cfg(unix)]
#[path = "cache_inventory.rs"]
mod inventory;

#[path = "cache_preset.rs"]
mod preset;

struct Fixture {
    temp: TempDir,
    root: PathBuf,
    cache: PathBuf,
    count: PathBuf,
}

impl Fixture {
    fn new() -> Self {
        let temp = tempfile::tempdir().unwrap();
        let root = temp.path().join("source");
        let cache = temp.path().join("cache");
        let count = temp.path().join("compile.log");
        fs::create_dir(&root).unwrap();
        let launcher = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("count_compile.py");
        fs::write(
            root.join("CMakeLists.txt"),
            format!(
                r##"
cmake_minimum_required(VERSION 3.15)
project(cache_fixture LANGUAGES C CXX)
set(CMAKE_CXX_COMPILER_LAUNCHER "{};{}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/src")
add_library(lbug STATIC fixture.cpp)
file(MAKE_DIRECTORY "${{CMAKE_BINARY_DIR}}/src/include")
file(WRITE "${{CMAKE_BINARY_DIR}}/src/include/generated.h" "#define GENERATED 1\n")
file(WRITE "${{CMAKE_BINARY_DIR}}/src/include/preset.h"
  "profile=${{CMAKE_BUILD_TYPE}}\n"
  "c_debug=${{CMAKE_C_FLAGS_DEBUG}}\n"
  "cxx_debug=${{CMAKE_CXX_FLAGS_DEBUG}}\n"
  "c_flags=${{CMAKE_C_FLAGS}}\n"
  "cxx_flags=${{CMAKE_CXX_FLAGS}}\n"
  "c_release=${{CMAKE_C_FLAGS_RELEASE}}\n"
  "cxx_release=${{CMAKE_CXX_FLAGS_RELEASE}}\n"
  "c_compiler=${{CMAKE_C_COMPILER}}\n"
  "cxx_compiler=${{CMAKE_CXX_COMPILER}}\n")
"##,
                env::var("PYTHON")
                    .unwrap_or_else(|_| "python3".into())
                    .replace('\\', "/"),
                launcher.to_string_lossy().replace('\\', "/")
            ),
        )
        .unwrap();
        fs::write(root.join("fixture.cpp"), "int fixture() { return 1; }\n").unwrap();
        let host =
            String::from_utf8(Command::new("rustc").arg("-vV").output().unwrap().stdout).unwrap();
        let host = host
            .lines()
            .find_map(|line| line.strip_prefix("host: "))
            .unwrap();
        for (name, value) in [
            ("TARGET", host),
            ("HOST", host),
            ("PROFILE", "release"),
            ("OPT_LEVEL", "3"),
            ("DEBUG", "false"),
            ("NUM_JOBS", "3"),
        ] {
            env::set_var(name, value);
        }
        env::set_var("CARGO_CFG_TARGET_OS", env::consts::OS);
        env::set_var("CARGO_CFG_TARGET_ARCH", env::consts::ARCH);
        env::remove_var("LBUG_REUSE_CMAKE_BUILD");
        env::remove_var("CXXFLAGS");
        env::set_var("LBUG_SOURCE_DIR", &root);
        env::set_var("LBUG_NATIVE_CACHE_DIR", &cache);
        env::set_var("COMPILE_LOG", &count);
        Self {
            temp,
            root,
            cache,
            count,
        }
    }

    fn build(&self, target: &str) -> Vec<PathBuf> {
        env::set_var("LBUG_SOURCE_DIR", &self.root);
        let out = self.temp.path().join(target).join("build/lbug-fixture/out");
        fs::create_dir_all(&out).unwrap();
        env::set_var("OUT_DIR", &out);
        super::build_bundled_cmake()
    }

    fn compiles(&self) -> usize {
        fs::read_to_string(&self.count).unwrap().lines().count()
    }

    fn entry(&self) -> PathBuf {
        let entries: Vec<_> = fs::read_dir(&self.cache)
            .unwrap_or_else(|error| panic!("cache was not published: {error}"))
            .map(|entry| entry.unwrap().path())
            .filter(|path| {
                path.file_name()
                    .unwrap()
                    .to_string_lossy()
                    .starts_with("entry-")
            })
            .collect();
        assert_eq!(entries.len(), 1, "one completed cache entry must publish");
        assert!(
            entries[0].join("manifest.sha256").is_file(),
            "entry has no manifest"
        );
        entries[0].clone()
    }
}

#[test]
fn two_target_directories_reuse_complete_output() {
    let fixture = Fixture::new();
    fixture.build("target-a");
    assert_eq!(fixture.compiles(), 1);
    fixture.build("target-b");
    if cfg!(unix) {
        assert_eq!(
            fixture.compiles(),
            1,
            "second target directory compiled C++"
        );
        let entry = fixture.entry();
        let top: Vec<_> = fs::read_dir(entry)
            .unwrap()
            .map(|item| item.unwrap().file_name())
            .collect();
        assert_eq!(top.len(), 2, "cache contains non-engine outputs");
        assert!(top.contains(&"manifest.sha256".into()) && top.contains(&"build".into()));
    } else {
        assert_eq!(fixture.compiles(), 2, "Windows must build from source");
        assert!(!fixture.cache.exists(), "Windows cache must be disabled");
    }
}

#[cfg(unix)]
#[test]
fn source_target_profile_and_flags_invalidate() {
    let fixture = Fixture::new();
    fixture.build("target-a");
    fixture.entry();
    fs::write(
        fixture.root.join("fixture.cpp"),
        "int fixture() { return 2; }\n",
    )
    .unwrap();
    fixture.build("target-source");
    assert_eq!(fixture.compiles(), 2, "source digest was ignored");
    let target = env::var("TARGET")
        .unwrap()
        .replace("-unknown-", "-cachetest-")
        .replace("-apple-", "-cachetest-");
    env::set_var("TARGET", target);
    fixture.build("target-triple");
    assert_eq!(fixture.compiles(), 3, "target triple was ignored");
    env::set_var("PROFILE", "debug");
    env::set_var("OPT_LEVEL", "0");
    env::set_var("DEBUG", "true");
    fixture.build("target-profile");
    assert_eq!(fixture.compiles(), 4, "profile was ignored");
    env::set_var("CXXFLAGS", "-DCACHE_FLAG=1");
    fixture.build("target-flags");
    assert_eq!(fixture.compiles(), 5, "native flag was ignored");
    env::set_var("CARGO_ENCODED_RUSTFLAGS", "--cfg\u{1f}mutants");
    fixture.build("target-rustflags");
    assert_eq!(
        fixture.compiles(),
        6,
        "mutation/coverage flags were ignored"
    );
    env::remove_var("CARGO_ENCODED_RUSTFLAGS");
    env::set_var("CARGO_FEATURE_ARROW", "1");
    fixture.build("target-feature");
    assert_eq!(fixture.compiles(), 7, "feature was ignored");
    env::remove_var("CARGO_FEATURE_ARROW");
}

#[cfg(unix)]
#[test]
fn partial_entry_is_refused_without_overwrite() {
    let fixture = Fixture::new();
    fixture.build("target-a");
    let entry = fixture.entry();
    fs::remove_file(entry.join("manifest.sha256")).unwrap();
    fixture.build("target-b");
    assert_eq!(fixture.compiles(), 2, "partial entry was used");
    assert!(
        !entry.join("manifest.sha256").exists(),
        "partial entry was overwritten"
    );
}

#[cfg(unix)]
#[test]
fn digest_mismatch_is_refused_without_overwrite() {
    let fixture = Fixture::new();
    fixture.build("target-a");
    let entry = fixture.entry();
    let archive = entry.join("build/src/liblbug.a");
    fs::write(&archive, b"planted bytes").unwrap();
    fixture.build("target-b");
    assert_eq!(fixture.compiles(), 2, "digest mismatch was used");
    assert_eq!(
        fs::read(archive).unwrap(),
        b"planted bytes",
        "entry was overwritten"
    );
}

#[cfg(unix)]
#[test]
fn group_writable_entry_is_refused() {
    use std::os::unix::fs::PermissionsExt;
    let fixture = Fixture::new();
    fixture.build("target-a");
    let entry = fixture.entry();
    fs::set_permissions(&entry, fs::Permissions::from_mode(0o770)).unwrap();
    fixture.build("target-b");
    assert_eq!(fixture.compiles(), 2, "group-writable entry was used");
    assert_eq!(
        fs::metadata(entry).unwrap().permissions().mode() & 0o777,
        0o770
    );
}

#[test]
fn unset_cache_keeps_source_default() {
    let fixture = Fixture::new();
    env::remove_var("LBUG_NATIVE_CACHE_DIR");
    fixture.build("target-a");
    fixture.build("target-b");
    assert_eq!(fixture.compiles(), 2);
    assert!(!fixture.cache.exists());
}

#[cfg(unix)]
#[test]
fn concurrent_population_never_overwrites() {
    let fixture = Fixture::new();
    let executable = env::current_exe().unwrap();
    let mut children = Vec::new();
    for name in ["target-a", "target-b"] {
        let out = fixture.temp.path().join(name);
        fs::create_dir(&out).unwrap();
        children.push(
            Command::new(&executable)
                .args([
                    "--ignored",
                    "--exact",
                    "cache_tests::population_worker",
                    "--nocapture",
                ])
                .env("OUT_DIR", out)
                .spawn()
                .unwrap(),
        );
    }
    for mut child in children {
        assert!(child.wait().unwrap().success());
    }
    fixture.entry();
    let count = fixture.compiles();
    assert!((1..=2).contains(&count));
    fixture.build("target-c");
    assert_eq!(fixture.compiles(), count, "concurrent entry was incomplete");
}

#[cfg(unix)]
#[test]
#[ignore = "subprocess worker for concurrent population"]
fn population_worker() {
    super::build_bundled_cmake();
}

#[cfg(unix)]
#[test]
fn explicit_toolchain_file_bypasses_read_and_publish() {
    let fixture = Fixture::new();
    let toolchain = fixture.temp.path().join("toolchain.cmake");
    fs::write(&toolchain, "# An otherwise compatible external toolchain\n").unwrap();
    let variable = format!("CMAKE_TOOLCHAIN_FILE_{}", env::var("TARGET").unwrap());
    let out = fixture.temp.path().join("toolchain-output");
    fs::create_dir(&out).unwrap();
    let output = Command::new(env::current_exe().unwrap())
        .args([
            "--ignored",
            "--exact",
            "cache_tests::population_worker",
            "--nocapture",
        ])
        .env(&variable, toolchain)
        .env("OUT_DIR", out)
        .output()
        .unwrap();
    assert!(output.status.success());
    let stdout = String::from_utf8(output.stdout).unwrap();
    assert!(
        stdout.contains(&format!("native cache disabled by {variable}")),
        "toolchain bypass warning missing: {stdout}"
    );
    assert!(
        !fixture.cache.exists(),
        "external toolchain populated/read cache root"
    );
    assert_eq!(fixture.compiles(), 1);
}

#[cfg(unix)]
#[test]
fn untrusted_root_fails_clearly() {
    use std::os::unix::fs::PermissionsExt;
    let fixture = Fixture::new();
    fs::create_dir(&fixture.cache).unwrap();
    fs::set_permissions(&fixture.cache, fs::Permissions::from_mode(0o770)).unwrap();
    let result = super::native_cache::NativeCache::from_env(&fixture.root);
    let error = result
        .err()
        .expect("group-writable root was accepted")
        .to_string();
    assert!(
        error.contains("untrusted LBUG_NATIVE_CACHE_DIR"),
        "unclear refusal: {error}"
    );
    assert_eq!(fs::read_dir(&fixture.cache).unwrap().count(), 0);
}

#[cfg(unix)]
#[test]
fn empty_existing_entry_is_not_replaced() {
    let fixture = Fixture::new();
    fixture.build("target-a");
    let entry = fixture.entry();
    fs::remove_dir_all(&entry).unwrap();
    fs::create_dir(&entry).unwrap();
    fixture.build("target-b");
    assert_eq!(fixture.compiles(), 2);
    assert_eq!(
        fs::read_dir(entry).unwrap().count(),
        0,
        "empty entry was replaced"
    );
}

#[cfg(unix)]
#[test]
fn unsupported_publication_keeps_private_output() {
    let fixture = Fixture::new();
    env::set_var("OUT_DIR", fixture.temp.path().join("out"));
    let cache = super::native_cache::NativeCache::from_env(&fixture.root)
        .unwrap()
        .unwrap();
    let output = cache
        .get_or_build_with(
            |private| {
                cmake::Config::new(&fixture.root)
                    .out_dir(private)
                    .no_build_target(true)
                    .build()
            },
            |_, _| Err(std::io::Error::from_raw_os_error(libc::EINVAL)),
        )
        .unwrap();
    assert!(output.join("build/src/liblbug.a").is_file());
    assert!(output.join("manifest.sha256").is_file());
    assert!(
        !fs::read_dir(&fixture.cache).unwrap().any(|entry| entry
            .unwrap()
            .file_name()
            .to_string_lossy()
            .starts_with("entry-")),
        "unsupported no-replace fell back to publication"
    );
    assert_eq!(fixture.compiles(), 1);
}

#[cfg(unix)]
#[test]
fn failed_native_build_does_not_publish() {
    let fixture = Fixture::new();
    fs::write(fixture.root.join("fixture.cpp"), "not valid C++\n").unwrap();
    assert!(std::panic::catch_unwind(|| fixture.build("target-a")).is_err());
    assert_eq!(
        fs::read_dir(&fixture.cache).unwrap().count(),
        0,
        "failed output was retained/published"
    );
}

#[cfg(unix)]
#[test]
fn compiler_executable_content_invalidates_with_same_version() {
    use std::os::unix::fs::PermissionsExt;
    let fixture = Fixture::new();
    let wrapper = fixture.temp.path().join("compiler");
    let saved = env::var_os("CXX");
    fs::write(&wrapper, "#!/bin/sh\nexec c++ \"$@\"\n").unwrap();
    fs::set_permissions(&wrapper, fs::Permissions::from_mode(0o700)).unwrap();
    env::set_var("CXX", &wrapper);
    fixture.build("target-a");
    fixture.entry();
    fs::write(
        &wrapper,
        "#!/bin/sh\n# changed executable, same --version\nexec c++ \"$@\"\n",
    )
    .unwrap();
    fixture.build("target-b");
    assert_eq!(fixture.compiles(), 2, "compiler content digest was ignored");
    if let Some(value) = saved {
        env::set_var("CXX", value);
    } else {
        env::remove_var("CXX");
    }
}

#[cfg(unix)]
#[test]
fn external_file_search_roots_bypass_cache() {
    let fixture = Fixture::new();
    for name in ["CMAKE_PREFIX_PATH", "CPATH"] {
        let saved = env::var_os(name);
        env::set_var(name, &fixture.root);
        let result = super::native_cache::NativeCache::from_env(&fixture.root).unwrap();
        if let Some(value) = saved {
            env::set_var(name, value);
        } else {
            env::remove_var(name);
        }
        assert!(
            result.is_none(),
            "external file search root {name} was cached"
        );
        assert!(!fixture.cache.exists(), "bypass accessed the cache root");
    }
}

#[cfg(unix)]
#[test]
fn other_user_owned_root_is_refused() {
    use std::os::unix::fs::MetadataExt;
    let fixture = Fixture::new();
    // SAFETY: geteuid has no memory preconditions.
    let uid = unsafe { libc::geteuid() };
    if fs::metadata("/").unwrap().uid() == uid {
        return;
    }
    env::set_var("LBUG_NATIVE_CACHE_DIR", "/");
    let result = super::native_cache::NativeCache::from_env(&fixture.root);
    assert!(result.is_err(), "another user's cache root was accepted");
}

#[cfg(unix)]
#[test]
fn linked_entry_is_refused_without_following_it() {
    use std::os::unix::fs::symlink;
    let fixture = Fixture::new();
    fixture.build("target-a");
    let entry = fixture.entry();
    let original = fixture.temp.path().join("planted");
    fs::rename(&entry, &original).unwrap();
    symlink(&original, &entry).unwrap();
    fixture.build("target-b");
    assert_eq!(fixture.compiles(), 2, "linked entry was followed");
    assert!(entry.is_symlink(), "linked entry was overwritten");
}

#[cfg(all(target_os = "linux", target_arch = "x86_64"))]
#[test]
fn real_bootstrap_snapshots_reuse_one_verified_native_build() {
    use crate::cfg_snapshots::{self, BOOTSTRAP, COVERAGE, NORMAL, SEMVER};
    let fixture = Fixture::new();
    let _cfgs = cfg_snapshots::Inputs::new();
    cfg_snapshots::apply(NORMAL, "");
    let cold = fixture.build("ordinary");
    assert_eq!(fixture.compiles(), 1);
    let entry = fixture.entry();
    for (target, snapshot, flags) in [
        ("coverage", NORMAL, COVERAGE),
        ("semver", BOOTSTRAP, SEMVER),
    ] {
        cfg_snapshots::apply(snapshot, flags);
        let hit = fixture.build(target);
        assert_eq!(fixture.compiles(), 1, "{target} rebuilt the C++ engine");
        assert_eq!(fixture.entry(), entry, "{target} created a different key");
        assert_ne!(hit[2], cold[2], "cache artifacts must be copied to OUT_DIR");
        assert_eq!(
            fs::read(hit[2].join("preset.h")).unwrap(),
            fs::read(cold[2].join("preset.h")).unwrap()
        );
    }
    // A compatible key must still verify the artifacts, not trust a hit stamp.
    let archive = entry.join("build/src/liblbug.a");
    fs::write(&archive, b"corrupt artifact").unwrap();
    fixture.build("semver-corrupt");
    assert_eq!(
        fixture.compiles(),
        2,
        "semver reused an unverified artifact"
    );
    assert_eq!(fs::read(archive).unwrap(), b"corrupt artifact");
}
