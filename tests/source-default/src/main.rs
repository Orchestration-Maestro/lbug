use lbug::{Connection, Database, SystemConfig, Value};

fn main() -> Result<(), lbug::Error> {
    assert_eq!(lbug::get_library_source(), "source");
    assert_eq!(lbug::get_library_dir(), None);
    let database = Database::new(
        ":memory:",
        SystemConfig::default()
            .buffer_pool_size(64 * 1024 * 1024)
            .max_num_threads(2),
    )?;
    let connection = Connection::new(&database)?;
    let mut result = connection.query("RETURN 1")?;
    assert_eq!(result.next(), Some(vec![Value::Int64(1)]));
    assert_eq!(result.next(), None);
    println!("bundled source linked; RETURN 1 succeeded");
    Ok(())
}
