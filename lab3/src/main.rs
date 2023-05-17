use lab3::ThreadPool;
use lab3::{InputHandler, OutputHandler, Status};
use std::net::{TcpListener, TcpStream};

fn main() {
    let listener = TcpListener::bind("0.0.0.0:8000").unwrap();

    let pool = ThreadPool::new(256);

    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                pool.send(|| handle_connection(stream));
            }
            Err(e) => {
                eprintln!("Error while establishing connection: {}", e);
            }
        }
    }
}

fn handle_connection(mut stream: TcpStream) {
    // println!("Connection established!");
    let got_path = InputHandler::get_path(&mut stream);
    let status = Status::from_path_result(got_path);
    // println!("Respond status code: {:?}", status.code);
    OutputHandler::output(&mut stream, status);
}
