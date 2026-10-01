use crate::RootDirectory;
#[cfg(unix)]
use crate::{Connection, Database, SystemConfig, Value};

#[cfg(unix)]
fn snapshot_outside(
    root: &std::path::Path,
    sibling: &std::path::Path,
) -> std::collections::BTreeMap<std::path::PathBuf, (u64, std::time::SystemTime)> {
    fn visit(
        path: &std::path::Path,
        root: &std::path::Path,
        entries: &mut std::collections::BTreeMap<std::path::PathBuf, (u64, std::time::SystemTime)>,
    ) {
        if path == root {
            return;
        }
        let metadata = std::fs::symlink_metadata(path).unwrap();
        entries.insert(
            path.to_path_buf(),
            (metadata.len(), metadata.modified().unwrap()),
        );
        if metadata.is_dir() {
            for child in std::fs::read_dir(path).unwrap() {
                visit(&child.unwrap().path(), root, entries);
            }
        }
    }
    let mut entries = std::collections::BTreeMap::new();
    visit(root.parent().unwrap(), root, &mut entries);
    visit(sibling, root, &mut entries);
    entries
}

#[cfg(unix)]
fn checked<T>(
    root: &std::path::Path,
    sibling: &std::path::Path,
    operation: impl FnOnce() -> Result<T, crate::Error>,
) -> Result<T, crate::Error> {
    let before = snapshot_outside(root, sibling);
    let result = operation();
    assert_eq!(
        before,
        snapshot_outside(root, sibling),
        "outside metadata changed"
    );
    result
}

#[cfg(unix)]
fn fixture() -> anyhow::Result<(tempfile::TempDir, std::path::PathBuf, std::path::PathBuf)> {
    use std::os::unix::fs::PermissionsExt;
    let directory = tempfile::tempdir()?;
    let root = directory.path().join("parent/root");
    let sibling = directory.path().join("outside");
    std::fs::create_dir_all(&root)?;
    std::fs::set_permissions(&root, std::fs::Permissions::from_mode(0o700))?;
    anyhow::ensure!(
        std::fs::metadata(&root)?.permissions().mode() & 0o7777 == 0o700,
        "fixture root is not exactly 0700"
    );
    std::fs::create_dir(&sibling)?;
    std::fs::write(sibling.join("sentinel"), b"outside sentinel")?;
    Ok((directory, root, sibling))
}

#[cfg(unix)]
#[test]
fn rooted_constructor_create_write_and_read_only_wal_reopen() -> anyhow::Result<()> {
    let (_directory, owned, sibling) = fixture()?;
    let root = checked(&owned, &sibling, || RootDirectory::open(&owned))?;
    let config = SystemConfig::default()
        .buffer_pool_size(16 * 1024 * 1024)
        .max_db_size(64 * 1024 * 1024)
        .max_num_threads(1)
        .auto_checkpoint(false);
    {
        let db = checked(&owned, &sibling, || {
            Database::new_rooted(&root, "normal.lbdb", config.clone())
        })?;
        let connection = Connection::new(&db)?;
        checked(&owned, &sibling, || {
            connection.query("CALL force_checkpoint_on_close=false")
        })?;
        checked(&owned, &sibling, || {
            connection.query("CREATE NODE TABLE Item(id INT64, PRIMARY KEY(id))")
        })?;
        checked(&owned, &sibling, || {
            connection.query("CREATE (:Item {id: 7})")
        })?;
    }
    {
        let db = checked(&owned, &sibling, || {
            Database::new_rooted(&root, "normal.lbdb", config.clone().read_only(true))
        })?;
        let connection = Connection::new(&db)?;
        assert_eq!(
            checked(&owned, &sibling, || connection
                .query("MATCH (i:Item) RETURN i.id"))?
            .next()
            .unwrap()[0],
            Value::Int64(7)
        );
    }
    {
        let db = checked(&owned, &sibling, || {
            Database::new_rooted(&root, "normal.lbdb", config.clone())
        })?;
        let connection = Connection::new(&db)?;
        checked(&owned, &sibling, || {
            connection.query("CALL force_checkpoint_on_close=false")
        })?;
        assert_eq!(
            checked(&owned, &sibling, || connection
                .query("MATCH (i:Item) RETURN i.id"))?
            .next()
            .unwrap()[0],
            Value::Int64(7)
        );
        checked(&owned, &sibling, || connection.query("BEGIN TRANSACTION"))?;
        checked(&owned, &sibling, || {
            connection.query("CREATE (:Item {id: 8})")
        })?;
        checked(&owned, &sibling, || connection.query("ROLLBACK"))?;
        checked(&owned, &sibling, || connection.query("CHECKPOINT"))?;
    }
    let before = std::fs::read_dir(&owned)?
        .map(|e| e.unwrap().file_name())
        .collect::<Vec<_>>();
    {
        let db = checked(&owned, &sibling, || {
            Database::new_rooted(&root, "normal.lbdb", config.read_only(true))
        })?;
        let connection = Connection::new(&db)?;
        assert_eq!(
            checked(&owned, &sibling, || connection
                .query("MATCH (i:Item) RETURN count(*)"))?
            .next()
            .unwrap()[0],
            Value::Int64(1)
        );
    }
    assert_eq!(
        before,
        std::fs::read_dir(&owned)?
            .map(|e| e.unwrap().file_name())
            .collect::<Vec<_>>()
    );
    for suffix in [
        "wal",
        "wal.checkpoint",
        "shadow",
        "checkpoint.intent.lock",
        "checkpoint.apply.lock",
    ] {
        assert!(!owned.join(format!("normal.lbdb.{suffix}")).exists());
    }
    assert!(!owned.join("normal.lbdb.tmp").exists());
    Ok(())
}

#[cfg(unix)]
#[test]
fn database_keeps_root_alive_after_caller_wrapper_drop() -> anyhow::Result<()> {
    let (_directory, owned, sibling) = fixture()?;
    let db = {
        let root = checked(&owned, &sibling, || RootDirectory::open(&owned))?;
        let config = SystemConfig::default()
            .buffer_pool_size(16 * 1024 * 1024)
            .max_db_size(64 * 1024 * 1024)
            .max_num_threads(1)
            .auto_checkpoint(false);
        checked(&owned, &sibling, || {
            Database::new_rooted(&root, "lifetime.lbdb", config)
        })?
    };
    let connection = Connection::new(&db)?;
    checked(&owned, &sibling, || {
        connection.query("CALL force_checkpoint_on_close=false")
    })?;
    checked(&owned, &sibling, || {
        connection.query("CREATE NODE TABLE Lifetime(id INT64, PRIMARY KEY(id))")
    })?;
    assert!(owned.join("lifetime.lbdb.wal").is_file());
    Ok(())
}

#[cfg(unix)]
#[test]
fn rooted_constructor_refuses_links_directories_and_invalid_names() -> anyhow::Result<()> {
    use std::os::unix::fs::{symlink, MetadataExt};
    let (_directory, owned, sibling) = fixture()?;
    let sentinel = sibling.join("sentinel");
    let before = std::fs::metadata(&sentinel)?;
    symlink(&sentinel, owned.join("link.lbdb"))?;
    std::fs::create_dir(owned.join("directory.lbdb"))?;
    let root = checked(&owned, &sibling, || RootDirectory::open(&owned))?;
    for name in [
        "link.lbdb",
        "directory.lbdb",
        "",
        ".",
        "..",
        "a/b",
        "a\\b",
        "a\0b",
        ":memory:",
    ] {
        assert!(
            checked(&owned, &sibling, || Database::new_rooted(
                &root,
                name,
                SystemConfig::default()
            ))
            .is_err(),
            "accepted {name:?}"
        );
    }
    let after = std::fs::metadata(&sentinel)?;
    assert_eq!((before.dev(), before.ino()), (after.dev(), after.ino()));
    assert_eq!(std::fs::read(&sentinel)?, b"outside sentinel");
    Ok(())
}

#[cfg(unix)]
#[test]
fn rooted_constructor_refuses_group_writable_root_without_creating_files() -> anyhow::Result<()> {
    use std::os::unix::fs::{MetadataExt, PermissionsExt};
    let (_directory, owned, sibling) = fixture()?;
    let sentinel = owned.join("sentinel");
    std::fs::write(&sentinel, b"private-root sentinel")?;
    std::fs::set_permissions(&owned, std::fs::Permissions::from_mode(0o770))?;
    let root_before = std::fs::metadata(&owned)?;
    let before = std::fs::metadata(&sentinel)?;
    let listing = || -> anyhow::Result<std::collections::BTreeSet<std::ffi::OsString>> {
        Ok(std::fs::read_dir(&owned)?
            .map(|entry| entry.map(|entry| entry.file_name()))
            .collect::<std::io::Result<_>>()?)
    };
    let entries = listing()?;
    for read_only in [false, true] {
        let result = checked(&owned, &sibling, || {
            let root = RootDirectory::open(&owned)?;
            Database::new_rooted(
                &root,
                "db.lbdb",
                SystemConfig::default()
                    .buffer_pool_size(16 * 1024 * 1024)
                    .max_db_size(64 * 1024 * 1024)
                    .max_num_threads(1)
                    .read_only(read_only),
            )
        });
        let error = match result {
            Err(error) => error,
            Ok(_) => panic!("group-writable root accepted"),
        };
        assert!(matches!(error, crate::Error::CxxException(_)));
        assert!(error
            .to_string()
            .contains("owned by the current user and not writable by group or others"));
        let root_after = std::fs::metadata(&owned)?;
        let after = std::fs::metadata(&sentinel)?;
        assert_eq!(
            (root_before.dev(), root_before.ino()),
            (root_after.dev(), root_after.ino())
        );
        assert_eq!((before.dev(), before.ino()), (after.dev(), after.ino()));
        assert_eq!(std::fs::read(&sentinel)?, b"private-root sentinel");
        assert_eq!(entries, listing()?);
        assert!(!owned.join("db.lbdb").exists());
    }
    Ok(())
}

#[cfg(unix)]
#[test]
fn root_directory_refuses_invalid_roots_without_lossy_conversion() -> anyhow::Result<()> {
    use std::os::unix::ffi::OsStringExt;
    let (_directory, owned, sibling) = fixture()?;
    assert!(checked(&owned, &sibling, || RootDirectory::open("relative-root")).is_err());
    assert!(checked(&owned, &sibling, || RootDirectory::open(
        owned.join("missing")
    ))
    .is_err());
    assert!(checked(&owned, &sibling, || RootDirectory::open("/nul\0root")).is_err());
    let invalid = std::ffi::OsString::from_vec(b"/invalid-utf8-\xff".to_vec());
    let error = checked(&owned, &sibling, || {
        RootDirectory::open(std::path::Path::new(&invalid))
    })
    .expect_err("root must not be converted lossily");
    assert!(matches!(error, crate::Error::InvalidRootPath));
    Ok(())
}

#[cfg(unix)]
#[test]
fn rooted_constructor_refuses_replaced_ancestor() -> anyhow::Result<()> {
    use std::os::unix::fs::symlink;
    let (directory, owned, sibling) = fixture()?;
    let root = checked(&owned, &sibling, || RootDirectory::open(&owned))?;
    std::fs::rename(owned.parent().unwrap(), directory.path().join("old-parent"))?;
    symlink(&sibling, owned.parent().unwrap())?;
    assert!(checked(&owned, &sibling, || Database::new_rooted(
        &root,
        "db.lbdb",
        SystemConfig::default()
    ))
    .is_err());
    assert!(!sibling.join("db.lbdb").exists());
    Ok(())
}

#[cfg(windows)]
#[test]
fn restricted_mode_fails_closed_on_windows() -> anyhow::Result<()> {
    let directory = tempfile::tempdir()?;
    let sentinel = directory.path().join("sentinel");
    std::fs::write(&sentinel, b"outside sentinel")?;
    let error = RootDirectory::open(directory.path()).expect_err("Windows must refuse");
    assert!(error.to_string().contains("unsupported"));
    assert_eq!(std::fs::read(sentinel)?, b"outside sentinel");
    Ok(())
}
