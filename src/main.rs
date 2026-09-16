use cascal::parser::parse;
use std::error::Error;
use std::io::{self, Write};
use std::path::PathBuf;
use std::process::ExitCode;

fn main() -> ExitCode {
    if let Err(error) = run() {
        eprintln!("{error}");
        return ExitCode::FAILURE;
    }
    ExitCode::SUCCESS
}

fn run() -> Result<(), Box<dyn Error>> {
    let mut args = std::env::args_os().skip(1);
    let path = PathBuf::from(args.next().unwrap_or_else(|| "source.txt".into()));
    if args.next().is_some() {
        return Err("usage: cascal [source-file]".into());
    }

    let source = std::fs::read(&path)
        .map_err(|e| io::Error::new(e.kind(), format!("{}: {e}", path.display())))?;

    let mut stdout = io::stdout().lock();
    stdout.write_all(&source)?;
    writeln!(stdout)?;

    let ast = parse(&source).map_err(|mut e| {
        e.file = path.display().to_string().into();
        e
    })?;

    let mut current = ast.root;
    while let Some(id) = current {
        ast.format_node(&mut stdout, id)?;
        writeln!(stdout)?;
        current = ast.arena[id].next;
    }
    Ok(())
}
