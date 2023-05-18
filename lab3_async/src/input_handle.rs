use tokio::io::{Error, ErrorKind};

use tokio::net::TcpStream;

use tokio::io::{AsyncBufReadExt, BufReader};

pub struct InputHandler;

impl InputHandler {
    pub async fn get_path(stream: &mut TcpStream) -> Result<String, Error> {
        let first_line = Self::read_until_empty_line(stream).await?;
        let path = Self::parse_first_line(first_line)?;
        if !Self::path_inside(&path) {
            return Err(Error::new::<String>(
                ErrorKind::InvalidInput,
                format!("Path {} out of project directory", path),
            ));
        }
        Ok(path)
    }

    async fn read_until_empty_line(stream: &mut TcpStream) -> Result<String, Error> {
        let mut reader = BufReader::new(stream);
        let mut first_line = String::new();
        reader.read_line(&mut first_line).await?;
        first_line.pop();
        first_line.pop();
        if first_line.len() == 0 {
            return Err(Error::new::<String>(
                ErrorKind::InvalidInput,
                "Not enough non-empty lines".into(),
            ));
        }
        // We only need first line, but we have to read while line is not empty.
        let mut line = String::new();
        while let Ok(len) = reader.read_line(&mut line).await {
            // CRLF!
            if len == 2 {
                break;
            }
        }
        Ok(first_line)
    }

    fn parse_first_line(line: String) -> Result<String, Error> {
        let words: Vec<_> = line.split(' ').filter(|&word| !word.is_empty()).collect();
        // println!("First line words: {:?}", words);
        if words.len() != 3 {
            return Err(Error::new::<String>(
                ErrorKind::InvalidInput,
                "Wrong request format".into(),
            ));
        }
        if words[0] != "GET" {
            return Err(Error::new::<String>(
                ErrorKind::InvalidInput,
                "Method not implemented".into(),
            ));
        }
        if words[2] != "HTTP/1.0" {
            return Err(Error::new::<String>(
                ErrorKind::InvalidInput,
                "HTTP version not supported".into(),
            ));
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
