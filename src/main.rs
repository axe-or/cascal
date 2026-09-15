use cascal::parser::{node_format, parse};
use std::io::{self, Write};
use std::path::Path;
use std::process::ExitCode;

pub fn read_file_whole(path: &Path) -> io::Result<Vec<u8>> {
    std::fs::read(path)
}

pub fn entrypoint(path: &Path, writer: &mut impl Write) -> Result<(), Box<dyn std::error::Error>> {
    let source = read_file_whole(path)
        .map_err(|e| io::Error::new(e.kind(), format!("{}: {e}", path.display())))?;
    writer.write_all(&source)?;
    writeln!(writer)?;
    let ast = parse(&source).map_err(|mut e| {
        e.file = path.display().to_string();
        e
    })?;
    let mut current = ast.root;
    while let Some(id) = current {
        node_format(writer, &ast, id)?;
        writeln!(writer)?;
        current = ast.arena[id].next;
    }
    Ok(())
}

fn main() -> ExitCode {
    let mut args = std::env::args_os().skip(1);
    let path = args.next().unwrap_or_else(|| "source.txt".into());
    if args.next().is_some() {
        eprintln!("usage: cascal [source-file]");
        return ExitCode::FAILURE;
    }
    match entrypoint(Path::new(&path), &mut io::stdout().lock()) {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("{e}");
            ExitCode::FAILURE
        }
    }
}
