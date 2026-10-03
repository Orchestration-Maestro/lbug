use super::{env, fs, Fixture};

fn value<'a>(snapshot: &'a str, name: &str) -> &'a str {
    snapshot
        .lines()
        .find_map(|line| line.strip_prefix(&format!("{name}=")))
        .unwrap()
}

#[test]
fn debug_preset_without_external_toolchain() {
    for (profile, debug) in [("debug", "true"), ("release", "false")] {
        let fixture = Fixture::new();
        env::set_var("PROFILE", profile);
        env::set_var("OPT_LEVEL", "0");
        env::set_var("DEBUG", debug);
        assert!(crate::build_env::bypass_names()
            .iter()
            .all(|name| env::var_os(name).is_none()));
        let includes = fixture.build("target-debug");
        let snapshot = fs::read_to_string(includes[2].join("preset.h")).unwrap();
        assert_eq!(value(&snapshot, "profile"), "Debug");
        let expected = if cfg!(windows) {
            "/Ob0 /Od /RTC1"
        } else {
            "-O0"
        };
        for language in ["c", "cxx"] {
            assert_eq!(value(&snapshot, &format!("{language}_debug")), expected);
            let flags = value(&snapshot, &format!("{language}_flags"));
            assert!(
                !flags
                    .split_whitespace()
                    .any(|flag| { flag.starts_with("-g") || flag == "/Zi" || flag == "/Z7" }),
                "symbol flag in {language}: {flags}"
            );
            if cfg!(windows) {
                let compiler = value(&snapshot, &format!("{language}_compiler"));
                assert!(
                    compiler.ends_with("/cl.exe") || compiler == "cl",
                    "{compiler}"
                );
            }
        }
        fixture.build("target-debug-reused");
        if cfg!(unix) {
            assert_eq!(fixture.compiles(), 1, "bundled preset bypassed caching");
            fixture.entry();
        } else {
            assert_eq!(fixture.compiles(), 2);
            assert!(
                !fixture.cache.exists(),
                "Windows caching must remain disabled"
            );
        }
    }
}

#[test]
fn release_does_not_apply_debug_preset() {
    let fixture = Fixture::new();
    let includes = fixture.build("target-release");
    let snapshot = fs::read_to_string(includes[2].join("preset.h")).unwrap();
    assert_eq!(value(&snapshot, "profile"), "Release");
    for language in ["c", "cxx"] {
        let debug = value(&snapshot, &format!("{language}_debug"));
        assert!(
            debug.contains(if cfg!(windows) { "/Zi" } else { "-g" }),
            "Release changed the default Debug flags: {debug}"
        );
        let release = value(&snapshot, &format!("{language}_release"));
        assert!(release.contains("DNDEBUG"), "{release}");
        assert!(
            !release
                .split_whitespace()
                .any(|flag| flag == "-O0" || flag == "/Od"),
            "Debug optimization leaked into Release: {release}"
        );
    }
}

#[cfg(unix)]
#[test]
fn native_key_hashes_the_bundled_preset_definitions() {
    // The preset lives in build.rs, whose bytes must stay in the native key.
    let build = include_str!("../../build.rs");
    for name in [
        "CMAKE_C_FLAGS_DEBUG",
        "CMAKE_CXX_FLAGS_DEBUG",
        "CMAKE_C_COMPILER",
        "CMAKE_CXX_COMPILER",
    ] {
        assert!(build.contains(name), "preset definition missing: {name}");
    }
    let key = include_str!("../../build_support/cache_key.rs");
    assert!(
        key.contains("field(&mut hash, include_bytes!(\"../build.rs\"));"),
        "bundled preset is not hashed into the native key"
    );
}
