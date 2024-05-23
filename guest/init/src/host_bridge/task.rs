use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug, Clone)]
#[serde(rename_all = "camelCase")]
pub enum SubtaskType {
    Command,
    File
}

#[derive(Serialize, Deserialize, Debug, Clone)]
#[serde(rename_all = "camelCase")]
pub struct CommandSubtask {
    pub command: String,
    pub args: Vec<String>,

    pub exit_code: i32
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct FileSubtask {
    pub path: String,
    pub content: String
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Subtask {
    pub id: String,
    pub done: bool,

    #[serde[rename = "type"]]
    pub subtask_type: SubtaskType,


    #[serde(flatten)]
    pub command_subtask: Option<CommandSubtask>,
    #[serde(flatten)]
    pub file_subtask: Option<FileSubtask>,

}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Task {
    pub id: String,
    pub paths: Vec<Vec<String>>,
    pub subtasks: Vec<Subtask>
}