mod machine_manager;
mod host_bridge;

use std::env;
use std::os::unix::process::CommandExt;
use std::process::{exit, Command};
use anyhow::Result;
use crate::machine_manager::*;

#[cfg(not(debug_assertions))]
use std::process;

#[cfg(debug_assertions)]
use crate::host_bridge::{HostBridge, RequestType};

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
    
    
    #[cfg(debug_assertions)]
    if args[0].contains("vs_test") {
        let mut bridge = HostBridge::new()?;
        bridge.message_host(RequestType::DownloadTest)?;
        exit(0);
    }
    
    #[cfg(not(debug_assertions))]
    if process::id() != 1 {
        eprintln!("{}: must be run as PID 1", args[0]);
        exit(1);
    }
    if args.len() < 2 {
        eprintln!("usage: {} [target init system]", args[0]);
        exit(1);
    }

    if is_machine_initializated() {
        println!("Machine is initializated!");
    } else {
        println!("Machine is not initializated!");
        
        query_motd()?;
        query_hostname()?;

        set_machine_initializated(true)?;
    }
    mount_sys_dirs()?;
    set_hostname()?;


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