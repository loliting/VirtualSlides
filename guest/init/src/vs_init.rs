use std::env;
use std::os::unix::process::CommandExt;
use std::process::{exit, Command};
use anyhow::Result;
use vs_init::machine_manager::*;

use std::process;

const ENV: [(&str, &str); 2] = [
    ("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"),
    ("TERM", "xterm-256color")
];

fn main() -> Result<()> {
    let args: Vec<String> = env::args().collect();
    
    if args[0].contains("reboot") {
        reboot()?;
        exit(0);
    }
    if args[0].contains("poweroff") || args[0].contains("shutdown") {
        poweroff()?;
        exit(0);
    }


    if process::id() != 1 {
        eprintln!("{}: must be run as PID 1", args[0]);
        exit(1);
    }
    if args.len() < 2 {
        eprintln!("usage: {} [target init system]", args[0]);
        exit(1);
    }

    system_init()?;

    if is_first_boot() {
        first_boot_initialization()?;
    }

    /* Actual init system starts as a PID 1 */
    println!("{}: Starting {}...", args[0], args[1]);
    let exec_err = Command::new(&args[1])
        .env_clear()
        .envs(ENV)
        .args(&args[2..])
        .exec();
    eprintln!("exec() failed: {}", exec_err);

    exit(1);
}