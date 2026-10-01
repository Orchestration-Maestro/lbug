use super::{env, fs, Fixture};

#[test]
fn inventory_is_ordered_by_path_not_digest() {
    let fixture = Fixture::new();
    let cmake = fixture.root.join("CMakeLists.txt");
    let mut source = fs::read_to_string(&cmake).unwrap();
    // SHA256("1") starts 6b; SHA256("0") starts 5f. Digest ordering must
    // disagree with aaa.h/zzz.h ordering regardless of the archive's digest.
    source.push_str("\nfile(WRITE \"${CMAKE_BINARY_DIR}/src/include/aaa.h\" \"1\")\nfile(WRITE \"${CMAKE_BINARY_DIR}/src/include/zzz.h\" \"0\")\n");
    fs::write(cmake, source).unwrap();
    fixture.build("target-a");
    fixture.build("target-b");
    assert_eq!(
        fixture.compiles(),
        1,
        "valid path-ordered manifest was refused"
    );
}

#[test]
fn unused_search_roots_do_not_change_default_engine() {
    let fixture = Fixture::new();
    let saved = env::var_os("PKG_CONFIG_PATH");
    env::set_var("PKG_CONFIG_PATH", &fixture.root);
    fixture.build("target-a");
    env::set_var("PKG_CONFIG_PATH", fixture.temp.path().join("changed-root"));
    fixture.build("target-b");
    if let Some(saved) = saved {
        env::set_var("PKG_CONFIG_PATH", saved);
    } else {
        env::remove_var("PKG_CONFIG_PATH");
    }
    if cfg!(feature = "extension_installer") {
        assert_eq!(fixture.compiles(), 2);
        assert!(
            !fixture.cache.exists(),
            "consumed pkg-config root was cached"
        );
    } else {
        assert_eq!(
            fixture.compiles(),
            1,
            "unused pkg-config root changed engine identity"
        );
        fixture.entry();
    }
}

#[test]
fn search_roots_are_inputs_only_for_extension_installer() {
    let fixture = Fixture::new();
    for name in [
        "PKG_CONFIG_PATH",
        "PKG_CONFIG_LIBDIR",
        "OPENSSL_DIR",
        "OPENSSL_ROOT_DIR",
        "OpenSSL_ROOT",
    ] {
        let saved = env::var_os(name);
        env::set_var(name, &fixture.root);
        let result = crate::native_cache::NativeCache::from_env(&fixture.root).unwrap();
        if let Some(saved) = saved {
            env::set_var(name, saved);
        } else {
            env::remove_var(name);
        }
        assert_eq!(
            result.is_none(),
            cfg!(feature = "extension_installer"),
            "wrong consumption condition for {name}"
        );
        assert_eq!(
            crate::build_env::native_names().contains(&name.to_owned()),
            cfg!(feature = "extension_installer"),
            "wrong key input condition for {name}"
        );
    }
}

#[test]
fn extension_installer_feature_changes_cache_identity() {
    let fixture = Fixture::new();
    let saved = env::var_os("CARGO_FEATURE_EXTENSION_INSTALLER");
    env::remove_var("CARGO_FEATURE_EXTENSION_INSTALLER");
    fixture.build("target-off");
    env::set_var("CARGO_FEATURE_EXTENSION_INSTALLER", "1");
    fixture.build("target-on");
    if let Some(saved) = saved {
        env::set_var("CARGO_FEATURE_EXTENSION_INSTALLER", saved);
    } else {
        env::remove_var("CARGO_FEATURE_EXTENSION_INSTALLER");
    }
    assert_eq!(
        fixture.compiles(),
        2,
        "extension_installer feature was not keyed"
    );
    assert_eq!(fs::read_dir(&fixture.cache).unwrap().count(), 2);
}
