use std::cmp::min;
use std::env;
use std::os::unix::process::{CommandExt, ExitStatusExt};
use std::process::{exit, Command};
use which::which;
use path_absolutize::*;
use regex::Regex;


use vs_guest_tools::host_bridge::{CommandSubtask, HostBridge, Request, RequestType};
use vs_guest_tools::machine_manager::fix_term;

fn compare_args(regex_args: &Vec<String>, args: &Vec<String>) -> bool {
    if args.len() - 1 != regex_args.len() {
        return false;
    }
    
    let mut args = args[1..].to_vec();
    for regex in regex_args {
        let regex = match Regex::new(&regex) {
            Ok(regex) => regex,
            Err(e) => {
                eprintln!("vs_run: regex \"{}\" is not valid: {}", regex, e);
                return false;
            }
        };

        for i in 0..args.len() {
            if regex.is_match(&args[i]) {
                args.remove(i);
            }
        }
    }

    args.is_empty()
}

fn get_matching_command_subtask(args: Vec<String>) -> Option<CommandSubtask> {
    let mut hb = match HostBridge::new() { 
        Ok(hb) => hb,
        Err(_) => {
            return None;
        }
    };

    let response = match hb.message_host_simple(RequestType::GetTasks) {
        Ok(response) => response,
        Err(_) => {
            return None;
        }
    };
    let tasks = match response.tasks {
        Some(tasks) => tasks,
        None => {
            return None;
        }
    };

    let arg0 = match which(&args[0]) {
        Ok(p) => p,
        Err(_) => {
            return None;
        }
    };
    let arg0_absolute = match arg0.absolutize() {
        Ok(p) => p,
        Err(_) => {
            return None;
        }
    };

    for task in tasks {
        for subtask in task.subtasks {
            if subtask.command_subtask.is_none() {
                continue;
            }
            let subtask = subtask.command_subtask.unwrap();

            let subtask_cmd = match which(&subtask.command) {
                Ok(p) => p,
                Err(_) => {
                    continue;
                }
            };
            let subtask_cmd = match subtask_cmd.absolutize() {
                Ok(p) => p,
                Err(_) => {
                    continue;
                }
            };

            if arg0_absolute == subtask_cmd && compare_args(&subtask.args, &args) {
                return Some(subtask);
            }
        }
    }

    None
}

fn main() {
    let args: Vec<String> = env::args().collect();
    
    if args.len() < 3 {
        exit(0);
    }
    
    let mut cmd = Command::new(&args[1]);
    cmd.arg0(&args[2])
        .args(&args[3..])
        .env("VS_RUN", "1");
    
    if args[2].contains("sh") {
        _ = fix_term();
    }

    let subtask = get_matching_command_subtask(args[2..].to_vec());

    if subtask.is_some() {
        let subtask = subtask.unwrap();

        let mut child =  match cmd.spawn() {
            Ok(child) => child,
            Err(e) => {
                eprintln!("{}", e);
                exit(1);
            }
        };
        let exit_code = match child.wait() {
            Ok(exit_status) => exit_status.into_raw(),
            Err(e) =>  {
                eprintln!("{}", e);
                exit(1);
            }
        };

        if subtask.exit_code == exit_code {
            println!("TASK DONE!!!");
        }

        exit(min(exit_code, 0xFF));
    }
    let exec_err = cmd.exec();
    
    eprintln!("exec() failed: {}", exec_err);
    exit(1);
}