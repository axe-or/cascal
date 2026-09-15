use std::process::Command;

#[test]
fn default_source_runs() {
    let output = Command::new(env!("CARGO_BIN_EXE_cascal"))
        .current_dir(env!("CARGO_MANIFEST_DIR"))
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(String::from_utf8_lossy(&output.stdout).contains("(proc main () () (block"));
}

#[test]
fn missing_file_is_an_error() {
    let output = Command::new(env!("CARGO_BIN_EXE_cascal"))
        .arg("this-file-does-not-exist.cascal")
        .output()
        .unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("this-file-does-not-exist.cascal"));
}

#[test]
fn arguments_are_checked() {
    let output = Command::new(env!("CARGO_BIN_EXE_cascal"))
        .args(["one", "two"])
        .output()
        .unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("usage:"));
}

#[test]
fn reports_parse_errors_with_file_and_offset() {
    let output = Command::new(env!("CARGO_BIN_EXE_cascal"))
        .current_dir(env!("CARGO_MANIFEST_DIR"))
        .arg("tests/fixtures/invalid.txt")
        .output()
        .unwrap();
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr)
        .contains("tests/fixtures/invalid.txt:0 error[E0009]"));
}
