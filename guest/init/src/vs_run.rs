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

fn get_matching_command_subtasks(args: Vec<String>) -> Option<Vec<(CommandSubtask, String, String)>> {
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

    let mut ans: Vec<(CommandSubtask, String, String)> = Vec::new();
    for task in tasks {
        for subtask in task.subtasks {
            if subtask.command_subtask.is_none() {
                continue;
            }
            let command_subtask = subtask.command_subtask.unwrap();

            let subtask_arg0 = match which(&command_subtask.command) {
                Ok(p) => p,
                Err(_) => {
                    continue;
                }
            };
            let subtask_arg0 = match subtask_arg0.absolutize() {
                Ok(p) => p,
                Err(_) => {
                    continue;
                }
            };

            if arg0_absolute == subtask_arg0 && compare_args(&command_subtask.args, &args) {
                ans.push((command_subtask, task.id.clone(), subtask.id));
            }
        }
    }

    if ans.is_empty() {
        return None
    }

    Some(ans)
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

    let subtasks = get_matching_command_subtasks(args[2..].to_vec());
    if subtasks.is_some() {
        let mut child =  match cmd.spawn() {
            Ok(child) => child,
            Err(e) => {
                eprintln!("vs_run: {}", e);
                exit(1);
            }
        };
        let exit_code = match child.wait() {
            Ok(exit_status) => exit_status.into_raw(),
            Err(e) =>  {
                eprintln!("vs_run: {}", e);
                exit(1);
            }
        };

        let mut hb = HostBridge::new().unwrap();
        for subtask in subtasks.unwrap() {
            let (subtask, task_id, subtask_id) = subtask;
            if subtask.exit_code == exit_code {
                let request = Request {
                    request_type: RequestType::FinishSubtask,
                    task_id: Some(task_id),
                    subtask_id: Some(subtask_id)
                };
                match hb.message_host(request) {
                    Ok(_) => (),
                    Err(e) =>  {
                        eprintln!("vs_run: {}", e);
                        exit(1);
                    }
                };
            }
        }

        exit(min(exit_code, 0xFF));
    }
    let exec_err = cmd.exec();
    
    eprintln!("exec() failed: {}", exec_err);
    exit(1);
}