use std::env;
use std::os::unix::process::CommandExt;
use std::process::{exit, Command};

fn main() {
    let args: Vec<String> = env::args().collect();
    
    if args.len() < 3 {
        exit(0);
    }

    let exec_err = Command::new(&args[1])
        .arg0(&args[2])
        .args(&args[3..])
        .env("VS_RUN", "1")
        .exec();
    
    eprintln!("exec() failed: {}", exec_err);
    exit(1);
}