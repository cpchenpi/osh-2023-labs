use std::{fmt::Debug, path::Path};

pub struct Status {
    pub code: StatusCode,
    pub path: String,
}

#[derive(Debug, PartialEq, Eq)]
pub enum StatusCode {
    Status200,
    Status404,
    Status500,
}

impl Status {
    pub fn from_path_result<T: Debug>(r: Result<String, T>) -> Self {
        let code = match &r {
            Ok(path) => {
                let p = Path::new(path);
                if p.exists() {
                    if p.is_dir() {
                        eprintln!("Error handling input: path {} is directory", path);
                        StatusCode::Status500
                    } else {
                        StatusCode::Status200
                    }
                } else {
                    eprintln!("Error handling input: path {} does not exist", path);
                    StatusCode::Status404
                }
            }
            Err(_) => {
                eprintln!("Error handling input: {:?}", r);
                StatusCode::Status500
            }
        };
        match code {
            StatusCode::Status200 => Status {
                path: r.unwrap(),
                code,
            },
            _ => Status {
                path: String::from("."),
                code,
            },
        }
    }
    pub fn get_status_line(&self) -> &'static str {
        match self.code {
            StatusCode::Status200 => "HTTP/1.0 200 OK",
            StatusCode::Status404 => "HTTP/1.0 404 Not Found",
            StatusCode::Status500 => "HTTP/1.0 500 Internal Server Error",
        }
    }
}
