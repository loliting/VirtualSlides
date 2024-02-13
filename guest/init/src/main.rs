use std::env;
use std::os::unix::process::CommandExt;
use std::process::{self, exit, Command};
use std::fs::File;
use std::io::ErrorKind;

use serde_json::Value;

const CONFIG_PATH: &str = "/etc/vs.json";

fn is_machine_initializated() -> bool {
    let config = File::options()
        .write(true)
        .read(true)
        .open(CONFIG_PATH);
    
    if config.is_err(){
        let err = config.unwrap_err();
        if err.kind() != ErrorKind::NotFound {
            eprintln!("Failed to open: {}: {}", CONFIG_PATH, err.to_string());
        }
        return false;
    }

    let config = config.unwrap();
    
    let v = serde_json::from_reader(config);
    if v.is_err() {
        eprintln!("Failed to parse {}: {}", CONFIG_PATH, v.unwrap_err().to_string());
        return false;
    }
    let v: Value = v.unwrap();

    let is_initializated = &v["initializated"];
    if is_initializated.is_boolean() == false {
        eprintln!("Failed to parse {}: {}", CONFIG_PATH, "Field \"initializated\" is not a boolean.");
        return false;
    }

    is_initializated.as_bool().unwrap()
}


fn main() {
    let args: Vec<String> = env::args().collect();

    if process::id() != 1 {
        eprintln!("{}: must be run as PID 1", args[0]);
        exit(1);
    }
    
    if args.len() != 2 {
        eprintln!("usage: {} [target init system]", args[0]);
        exit(1);
    }
    
    if is_machine_initializated() {
        println!("Machine is initializated!");
    }
    else {
        println!("Machine is not initializated!");
    }


    let env = [
        ("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"),
        ("TERM", "xterm-256color")
    ];

    /* Actual init system starts as a PID 1 */
    println!("{}: Starting {}...", args[0], args[1]);
    let exec_err = Command::new(&args[1]).env_clear().envs(env).exec();
    eprintln!("exec() failed: {}", exec_err);

    exit(1);
}