use super::{env, fs, Fixture};
use std::os::unix::fs::PermissionsExt;

#[test]
fn replaced_trusted_root_is_refused() {
    let fixture = Fixture::new();
    fixture.build("target-a");
    let cache = crate::native_cache::NativeCache::from_env(&fixture.root)
        .unwrap()
        .unwrap();
    fs::rename(&fixture.cache, fixture.temp.path().join("saved-cache")).unwrap();
    fs::create_dir(&fixture.cache).unwrap();
    fs::set_permissions(&fixture.cache, fs::Permissions::from_mode(0o700)).unwrap();
    let result = cache.get_or_build(|_| panic!("replaced root must not build"));
    assert!(
        result.is_err(),
        "different trusted root inode was accepted: {result:?}"
    );
}

#[test]
fn writable_held_manifest_is_refused() {
    let fixture = Fixture::new();
    fixture.build("target-a");
    fs::set_permissions(
        fixture.entry().join("manifest.sha256"),
        fs::Permissions::from_mode(0o660),
    )
    .unwrap();
    fixture.build("target-b");
    assert_eq!(
        fixture.compiles(),
        2,
        "writable manifest handle was trusted"
    );
}

#[test]
fn writable_held_artifact_is_refused() {
    let fixture = Fixture::new();
    fixture.build("target-a");
    fs::set_permissions(
        fixture.entry().join("build/src/liblbug.a"),
        fs::Permissions::from_mode(0o660),
    )
    .unwrap();
    fixture.build("target-b");
    assert_eq!(
        fixture.compiles(),
        2,
        "writable artifact handle was trusted"
    );
}

#[test]
fn writable_held_ancestor_is_refused() {
    let fixture = Fixture::new();
    fixture.build("target-a");
    fs::set_permissions(
        fixture.entry().join("build/src"),
        fs::Permissions::from_mode(0o770),
    )
    .unwrap();
    fixture.build("target-b");
    assert_eq!(
        fixture.compiles(),
        2,
        "writable ancestor handle was trusted"
    );
}

#[test]
fn publication_uses_held_root_after_path_replacement() {
    let fixture = Fixture::new();
    env::set_var("OUT_DIR", fixture.temp.path().join("out"));
    let saved = fixture.temp.path().join("saved-cache");
    let cache = crate::native_cache::NativeCache::from_env(&fixture.root)
        .unwrap()
        .unwrap();
    let switched = std::cell::Cell::new(false);
    let output = cache
        .get_or_build_with_sync(
            |private| {
                cmake::Config::new(&fixture.root)
                    .out_dir(private)
                    .no_build_target(true)
                    .build()
            },
            |file| {
                if !switched.replace(true) {
                    // Swap the pathname after staging starts, before rename.
                    fs::rename(&fixture.cache, &saved)?;
                    fs::create_dir(&fixture.cache)?;
                }
                file.sync_all()
            },
        )
        .unwrap();
    assert!(output.join("build/src/liblbug.a").is_file());
    assert_eq!(
        fs::read_dir(&fixture.cache).unwrap().count(),
        0,
        "publication escaped the held root"
    );
    assert!(
        fs::read_dir(saved).unwrap().any(|item| item
            .unwrap()
            .file_name()
            .to_string_lossy()
            .starts_with("entry-")),
        "publication did not use the held root"
    );
}
