use std::{net::{TcpListener, TcpStream}, io::{BufReader, BufRead, Write}, path::{Path}};

fn main() {
    let listener  = TcpListener::bind("0.0.0.0:8000").unwrap();
    for stream in listener.incoming() {
        handle_connection(stream.unwrap());
    }
}

#[derive(Debug)]
enum Status {
    Status200,
    Status404,
    Status500,
}

fn handle_connection(mut stream: TcpStream){
    println!("Connection established!");
    let reader = BufReader::new(&mut stream);
    // We only need first line, but we have to read while line is not empty.
    let lines:Vec<_> = reader
        .lines()
        .map(|line| line.unwrap())
        .take_while(|line| !line.is_empty())
        .collect();
    println!("Lines: {:?}", lines);
    assert!(lines.len() > 0);
    let first_line = &lines[0];
    println!("First line: {}", first_line);
    let words:Vec<_> = first_line
        .split(' ')
        .filter(|&word| !word.is_empty())
        .collect();
    assert!(words.len() == 3);
    println!("Words: {:?}", words);
    let mut legal = words[0] == "GET" && words[2] == "HTTP/1.0";
    
    let path = &(String::from(".") + words[1]);
    if Path::new(path).is_dir() || !path_inside(path) {
        legal = false;
    }
    let status= if !legal {
        Status::Status500
    } else {
        if Path::new(path).exists() {
            Status::Status200
        } else {
            Status::Status404
        }
    };
    println!("Respond status: {:?}", status);
    let status_line = match status {
        Status::Status200 => {"HTTP/1.0 200 OK"},
        Status::Status404 => {"HTTP/1.0 404 Not Found"},
        Status::Status500 => {"HTTP/1.0 500 Internal Server Error"},
    };
    let mut respond_content:Vec<_> = Vec::new();
    respond_content.push("");
    write_line(&mut stream, status_line);
    for line in respond_content{
        write_line(&mut stream, line);
    }
}

fn write_line(stream: &mut TcpStream, s: &str){
    write_all(stream, format!("{}\r\n", s));
}

fn write_all(stream: &mut TcpStream, s: String){
    let mut written_count = 0;
    let n = s.len();
    let buf = s.as_bytes();
    while written_count < n {
        written_count +=  stream.write(&buf[written_count..n]).unwrap();
    }
}

// the path should not jump out of the project directory
fn path_inside(path: &str)->bool{
    let mut now_layer = 0;
    for word in path.split('/') {
        match word {
            "." => {},
            ".." => {
                if now_layer != 0 {
                    now_layer -= 1;
                } else {
                    return false;
                }
            },
            _ => {
                now_layer += 1;
            }
        }
    }
    true
}

#[cfg(test)]
mod test{
    use crate::*;

    #[test]
    fn test_path_inside() {
        assert!(!path_inside(".."));
        assert!(path_inside("index.html"));
        assert!(!path_inside("a/.././../a/a"));
    }
}