use super::*;

#[test]
fn native_hash_index_rollback_repro_matrix() -> anyhow::Result<()> {
    let mut failed = Vec::new();
    for (name, checkpoint_schema, committed, key, cycles) in [
        ("empty_short_control", false, false, "short".to_owned(), 0),
        ("fresh_short_rollback", false, false, "short".to_owned(), 1),
        ("fresh_long_rollback", false, false, "x".repeat(4096), 1),
        (
            "a_schema_checkpoint_fresh",
            true,
            false,
            "short".to_owned(),
            1,
        ),
        (
            "b_schema_checkpoint_committed",
            true,
            true,
            "short".to_owned(),
            1,
        ),
        (
            "c_schema_checkpoint_overflow",
            true,
            false,
            "x".repeat(4096),
            1,
        ),
        (
            "d_schema_checkpoint_cycles",
            true,
            true,
            "x".repeat(4096),
            4,
        ),
        (
            "committed_rollback_control",
            false,
            true,
            "short".to_owned(),
            1,
        ),
    ] {
        #[cfg(unix)]
        let (_directory, owned, _sibling) = fixture()?;
        #[cfg(windows)]
        let directory = windows_private_fixture()?;
        #[cfg(windows)]
        let owned = directory.path().to_path_buf();
        let root = RootDirectory::open(&owned)?;
        let config = SystemConfig::default()
            .buffer_pool_size(16 * 1024 * 1024)
            .max_db_size(64 * 1024 * 1024)
            .max_num_threads(1)
            .auto_checkpoint(false);
        let expected = i64::from(committed);
        {
            #[cfg(unix)]
            let db = Database::new_rooted(&root, "rows.lbdb", config.clone())?;
            // Windows rooted mode is read-only; prepare the file with the ordinary writer.
            #[cfg(windows)]
            let db = Database::new(owned.join("rows.lbdb"), config.clone())?;
            let conn = Connection::new(&db)?;
            conn.query("CREATE NODE TABLE Test(id STRING, PRIMARY KEY(id))")?;
            if checkpoint_schema {
                conn.query("CHECKPOINT")?;
            }
            let mut insert = conn.prepare("CREATE (:Test {id: $id})")?;
            if committed {
                conn.execute(&mut insert, vec![("id", Value::String("committed".into()))])?;
            }
            for n in 0..cycles {
                conn.query("BEGIN TRANSACTION")?;
                conn.execute(
                    &mut insert,
                    vec![("id", Value::String(format!("{key}{n}")))],
                )?;
                conn.query("ROLLBACK")?;
                if n > 0 {
                    conn.query("BEGIN TRANSACTION")?;
                    conn.execute(&mut insert, vec![("id", Value::String(format!("keep{n}")))])?;
                    conn.query("COMMIT")?;
                    conn.query("MATCH (e:Test) WHERE e.id STARTS WITH 'keep' DELETE e")?;
                }
            }
            assert_eq!(
                conn.query("MATCH (e:Test) RETURN count(e)")?
                    .next()
                    .unwrap(),
                [Value::Int64(expected)],
                "{name}: count before checkpoint"
            );
            conn.query("CHECKPOINT")?;
        }
        #[cfg(windows)]
        for entry in std::fs::read_dir(&owned)? {
            windows_set_fixture_file_owner(&entry?.path())?;
        }
        match Database::new_rooted(&root, "rows.lbdb", config.clone().read_only(true)) {
            Ok(db) => {
                let conn = Connection::new(&db)?;
                let row = conn
                    .query("MATCH (e:Test) RETURN count(e)")?
                    .next()
                    .unwrap();
                eprintln!("MATRIX {name}: REOPEN_OK count={row:?}");
                assert_eq!(row, [Value::Int64(expected)], "{name}: reopened count");
                if committed {
                    assert_eq!(
                        conn.query("MATCH (e:Test) WHERE e.id = 'committed' RETURN e.id")?
                            .next()
                            .unwrap(),
                        [Value::String("committed".into())],
                        "{name}: committed primary-key lookup"
                    );
                }
            }
            Err(error) => {
                eprintln!("MATRIX {name}: REOPEN_FAILED {error}");
                failed.push(name);
            }
        }
        if failed.last() == Some(&name) {
            continue;
        }
        let reused_key = format!("{key}0");
        {
            #[cfg(unix)]
            let db = Database::new_rooted(&root, "rows.lbdb", config.clone().read_only(false))?;
            #[cfg(windows)]
            let db = Database::new(owned.join("rows.lbdb"), config.clone().read_only(false))?;
            let conn = Connection::new(&db)?;
            conn.query("BEGIN TRANSACTION")?;
            let mut insert = conn.prepare("CREATE (:Test {id: $id})")?;
            conn.execute(&mut insert, vec![("id", Value::String(reused_key.clone()))])?;
            conn.query("COMMIT")?;
            conn.query("CHECKPOINT")?;
        }
        #[cfg(windows)]
        for entry in std::fs::read_dir(&owned)? {
            windows_set_fixture_file_owner(&entry?.path())?;
        }
        let db = Database::new_rooted(&root, "rows.lbdb", config.read_only(true))?;
        let conn = Connection::new(&db)?;
        assert_eq!(
            conn.query("MATCH (e:Test) RETURN count(e)")?
                .next()
                .unwrap(),
            [Value::Int64(expected + 1)],
            "{name}: exactly one new row after reusing the rolled-back key"
        );
        let mut lookup = conn.prepare("MATCH (e:Test) WHERE e.id = $id RETURN e.id")?;
        let mut rows =
            conn.execute(&mut lookup, vec![("id", Value::String(reused_key.clone()))])?;
        assert_eq!(rows.next().unwrap(), [Value::String(reused_key)]);
        assert!(rows.next().is_none(), "{name}: duplicate reused key");
        eprintln!("MATRIX {name}: KEY_REUSE_OK count={}", expected + 1);
    }
    assert!(failed.is_empty(), "native matrix failures: {failed:?}");
    Ok(())
}
