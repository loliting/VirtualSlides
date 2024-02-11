use std::env;
use std::os::unix::process::CommandExt;
use std::process::{exit, Command};

fn main() {
    let args: Vec<String> = env::args().collect();
    
    if args.len() != 2 {
        eprintln!("usage: {0} [target init system]", (&args)[0]);
        exit(1);
    }

    let env = [
        ("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"),
        ("TERM", "xterm-256color")
    ];

    /* Actual init system starts as a PID 1 */
    println!("{0}: Starting {1}...", args[0], args[1]);
    Command::new(args[1].clone()).env_clear().envs(env).exec();
    eprintln!("exec() failed!");

    exit(1);
}