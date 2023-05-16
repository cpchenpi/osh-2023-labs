use lab3::{input_handle::InputHandler, status::Status};
use std::{
    io::Write,
    net::{TcpListener, TcpStream},
};

fn main() {
    let listener = TcpListener::bind("0.0.0.0:8000").unwrap();
    for stream in listener.incoming() {
        handle_connection(stream.unwrap());
    }
}

fn handle_connection(mut stream: TcpStream) {
    println!("Connection established!");
    let got_path = InputHandler::get_path(&mut stream);
    let status = Status::from_path_result(got_path);
    println!("Respond status code: {:?}", status.code);
    let status_line = status.get_status_line();
    let mut respond_content: Vec<_> = Vec::new();
    respond_content.push("");
    write_line(&mut stream, status_line);
    for line in respond_content {
        write_line(&mut stream, line);
    }
}

fn write_line(stream: &mut TcpStream, s: &str) {
    write_all(stream, format!("{}\r\n", s));
}

fn write_all(stream: &mut TcpStream, s: String) {
    let mut written_count = 0;
    let n = s.len();
    let buf = s.as_bytes();
    while written_count < n {
        written_count += stream.write(&buf[written_count..n]).unwrap();
    }
}
