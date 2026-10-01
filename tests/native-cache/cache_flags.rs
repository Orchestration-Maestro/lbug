use super::{env, fs, Fixture};

#[test]
fn unknown_and_path_bearing_flags_bypass() {
    let fixture = Fixture::new();
    for flags in [
        "-fpass-plugin=x",
        "outside.cpp",
        "/outside/input",
        "-fprofile-instr-generate=/outside/profile",
        "-DCACHE_HEADER=/outside/header.h",
        "-Wl,/outside/library.a",
        "-Wa,@outside.rsp",
    ] {
        env::set_var("CXXFLAGS", flags);
        let result = crate::native_cache::NativeCache::from_env(&fixture.root);
        env::remove_var("CXXFLAGS");
        assert!(
            result.unwrap().is_none(),
            "unknown/path-bearing flags were keyed: {flags}"
        );
        assert!(!fixture.cache.exists(), "bypass accessed cache: {flags}");
    }
}

#[test]
fn scalar_flag_families_are_cached_and_keyed() {
    for (first, changed) in [
        ("-DCACHE_FLAG=1", "-DCACHE_FLAG=2"),
        ("-UFLAG_A", "-UFLAG_B"),
        ("-O0", "-O1"),
        ("-g0", "-g1"),
        ("-std=c++17", "-std=c++20"),
        ("-Wall", "-Wextra"),
        ("-fpic", "-fPIC"),
        ("-fno-exceptions", "-fexceptions"),
        ("-fno-omit-frame-pointer", "-fomit-frame-pointer"),
    ] {
        let fixture = Fixture::new();
        env::set_var("CXXFLAGS", first);
        assert!(
            crate::native_cache::NativeCache::from_env(&fixture.root)
                .unwrap()
                .is_some(),
            "scalar family bypassed: {first}"
        );
        fixture.build("target-a");
        env::set_var("CXXFLAGS", changed);
        fixture.build("target-b");
        env::remove_var("CXXFLAGS");
        assert_eq!(
            fixture.compiles(),
            2,
            "scalar value not keyed: {first} -> {changed}"
        );
        assert_eq!(
            fs::read_dir(&fixture.cache).unwrap().count(),
            2,
            "scalar family bypassed: {changed}"
        );
    }
}
