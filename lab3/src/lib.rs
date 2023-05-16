pub mod status {
    use std::path::Path;

    #[derive(Debug)]
    pub enum Status {
        Status200,
        Status404,
        Status500,
    }

    impl Status {
        pub fn from_path_result<T>(r: &Result<String, T>) -> Self {
            match r {
                Ok(path) => {
                    let p = Path::new(path);
                    if p.exists() {
                        if p.is_dir() {
                            Status::Status500
                        } else {
                            Status::Status200
                        }
                    } else {
                        Status::Status404
                    }
                }
                Err(_) => Status::Status500,
            }
        }
        pub fn get_status_line(&self) -> &'static str {
            match self {
                Status::Status200 => "HTTP/1.0 200 OK",
                Status::Status404 => "HTTP/1.0 404 Not Found",
                Status::Status500 => "HTTP/1.0 500 Internal Server Error",
            }
        }
    }
}

pub mod input_handle {
    use std::{
        error::Error,
        io::{BufRead, BufReader},
        net::TcpStream,
    };

    pub struct InputHandler;

    impl InputHandler {
        pub fn get_path(stream: &mut TcpStream) -> Result<String, Box<dyn Error>> {
            let first_line = Self::read_until_empty_line(stream)?;
            let path = Self::parse_first_line(first_line)?;
            if !Self::path_inside(&path) {
                return Err("Path out of project directory".into());
            }
            Ok(path)
        }

        fn read_until_empty_line(stream: &mut TcpStream) -> Result<String, Box<dyn Error>> {
            let reader = BufReader::new(stream);
            // We only need first line, but we have to read while line is not empty.
            let mut lines: Vec<_> = reader
                .lines()
                .map(|line| line.unwrap())
                .take_while(|line| !line.is_empty())
                .collect();
            println!("Got lines: {:?}", lines);
            if lines.len() == 0 {
                return Err("Not enough non-empty lines".into());
            }
            Ok(lines.remove(0))
        }

        fn parse_first_line(line: String) -> Result<String, Box<dyn Error>> {
            let words: Vec<_> = line.split(' ').filter(|&word| !word.is_empty()).collect();
            println!("First line words: {:?}", words);
            if words.len() != 3 {
                return Err("Wrong request format".into());
            }
            if words[0] != "GET" {
                return Err("Method not implemented".into());
            }
            if words[2] != "HTTP/1.0" {
                return Err("HTTP version not supported".into());
            }
            let res = ".".to_string() + words[1];
            println!("Parsed relative path: {}", res);
            Ok(res)
        }

        // the path should not jump out of the project directory
        fn path_inside(path: &str) -> bool {
            let mut now_layer = 0;
            for word in path.split('/') {
                match word {
                    "." => {}
                    ".." => {
                        if now_layer != 0 {
                            now_layer -= 1;
                        } else {
                            return false;
                        }
                    }
                    _ => {
                        now_layer += 1;
                    }
                }
            }
            true
        }
    }

    #[cfg(test)]
    mod test {
        use super::InputHandler;
        #[test]
        fn test_parse_first_line() {
            InputHandler::parse_first_line("POST / HTTP/1.0".into()).unwrap_err();
            InputHandler::parse_first_line("GET / HTTP/1.1".into()).unwrap_err();
            assert_eq!(
                InputHandler::parse_first_line("GET / HTTP/1.0".into()).unwrap(),
                "./"
            );
            assert_eq!(
                InputHandler::parse_first_line("GET /index.html HTTP/1.0".into()).unwrap(),
                "./index.html"
            );
        }

        #[test]
        fn test_path_inside() {
            assert!(!InputHandler::path_inside(".."));
            assert!(InputHandler::path_inside("index.html"));
            assert!(!InputHandler::path_inside("a/.././../a/a"));
        }
    }
}
