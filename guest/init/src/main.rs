mod info;
mod host_bridge;

use std::env;
use std::os::unix::process::CommandExt;
use std::process::{exit, Command};
use crate::info::machine::is_machine_initializated;
use crate::host_bridge::HostBridge;

#[cfg(not(debug_assertions))]
use std::process;


fn main() {
    let args: Vec<String> = env::args().collect();
    
    #[cfg(not(debug_assertions))]
    if process::id() != 1 {
        eprintln!("{}: must be run as PID 1", args[0]);
        exit(1);
    }
    
    if args.len() < 2 {
        eprintln!("usage: {} [target init system]", args[0]);
        exit(1);
    }
    _ = HostBridge::new("/dev/hvc1", "/dev/hvc0").unwrap();

    if is_machine_initializated() {
        println!("Machine is initializated!");
    } else {
        println!("Machine is not initializated!");
    }


    let env = [
        ("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"),
        ("TERM", "xterm-256color")
    ];

    /* Actual init system starts as a PID 1 */
    println!("{}: Starting {}...", args[0], args[1]);
    let exec_err = Command::new(&args[1])
        .env_clear()
        .envs(env)
        .args(&args[2..])
        .exec();
    eprintln!("exec() failed: {}", exec_err);

    exit(1);
}