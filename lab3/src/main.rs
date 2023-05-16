use lab3::{input_handle::InputHandler, output_handle::OutputHandler, status::Status};
use std::net::{TcpListener, TcpStream};

fn main() {
    let listener = TcpListener::bind("0.0.0.0:8000").unwrap();
    for stream in listener.incoming() {
        handle_connection(stream.unwrap());
    }
}

fn handle_connection(mut stream: TcpStream) {
    // println!("Connection established!");
    let got_path = InputHandler::get_path(&mut stream);
    let status = Status::from_path_result(got_path);
    // println!("Respond status code: {:?}", status.code);
    OutputHandler::output(&mut stream, status);
}
