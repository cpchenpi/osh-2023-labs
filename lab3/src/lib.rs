pub mod status {
    use std::{fmt::Debug, path::Path};

    pub struct Status {
        pub code: StatusCode,
        pub path: String,
    }

    #[derive(Debug)]
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
                return Err(format!("Path {} out of project directory", path).into());
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
            // println!("Got lines: {:?}", lines);
            if lines.len() == 0 {
                return Err("Not enough non-empty lines".into());
            }
            Ok(lines.remove(0))
        }

        fn parse_first_line(line: String) -> Result<String, Box<dyn Error>> {
            let words: Vec<_> = line.split(' ').filter(|&word| !word.is_empty()).collect();
            // println!("First line words: {:?}", words);
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
            // println!("Parsed relative path: {}", res);
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

pub mod output_handle {
    use std::{error::Error, fs::read, io::Write, net::TcpStream};

    use crate::status::{Status, StatusCode};

    pub struct OutputHandler;

    fn write_line(stream: &mut TcpStream, s: &str) -> Result<(), Box<dyn Error>> {
        write_all(stream, format!("{}\r\n", s))?;
        Ok(())
    }

    fn write_all(stream: &mut TcpStream, s: String) -> Result<(), Box<dyn Error>> {
        let mut written_count = 0;
        let n = s.len();
        let buf = s.as_bytes();
        while written_count < n {
            written_count += stream.write(&buf[written_count..n])?;
        }
        Ok(())
    }

    fn write_all_u8(stream: &mut TcpStream, buf: Vec<u8>) -> Result<(), Box<dyn Error>> {
        let mut written_count = 0;
        let n = buf.len();
        while written_count < n {
            written_count += stream.write(&buf[written_count..n])?;
        }
        Ok(())
    }

    impl OutputHandler {
        pub fn output(stream: &mut TcpStream, status: Status) {
            if let Err(e) = Self::handle(stream, status) {
                eprintln!("Error while handling output: {}", e);
            }
        }

        fn handle(stream: &mut TcpStream, status: Status) -> Result<(), Box<dyn Error>> {
            let status_line = status.get_status_line();
            write_line(stream, status_line)?;
            let buf = match status.code {
                StatusCode::Status200 => read(&status.path)?,
                _ => Vec::new(),
            };
            // println!("Output buf content: {:?}", buf);
            write_line(stream, &format!("Content-Length: {}", buf.len()))?;
            write_line(stream, "")?;
            write_all_u8(stream, buf)?;
            Ok(())
        }
    }
}
