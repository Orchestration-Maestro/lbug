use super::{fs, Fixture};
use crate::cfg_snapshots::{self, BOOTSTRAP, NORMAL, SEMVER};

#[test]
fn reviewer_bootstrap_cfg_reader_is_not_aliased() {
    let fixture = Fixture::new();
    let _cfgs = cfg_snapshots::Inputs::new();
    // Only this variant reads Cargo cfgs; the reuse fixture stays reader-free.
    let cmake = fixture.root.join("CMakeLists.txt");
    let mut source = fs::read_to_string(&cmake).unwrap();
    source.push_str(
        "\nfile(WRITE \"${CMAKE_BINARY_DIR}/src/include/bootstrap.h\" \"fmt=$ENV{CARGO_CFG_FMT_DEBUG}\\n\")\n",
    );
    fs::write(cmake, source).unwrap();
    cfg_snapshots::apply(NORMAL, "");
    let cold = fixture.build("ordinary");
    assert_eq!(fs::read(cold[2].join("bootstrap.h")).unwrap(), b"fmt=\n");
    cfg_snapshots::apply(BOOTSTRAP, SEMVER);
    let bootstrap = fixture.build("bootstrap");
    assert_eq!(
        fs::read(bootstrap[2].join("bootstrap.h")).unwrap(),
        b"fmt=full\n"
    );
    assert_eq!(fixture.compiles(), 2, "Cargo cfg reader must rebuild");
}
