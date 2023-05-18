use lab3_async::{InputHandler, OutputHandler, Status};
use tokio::net::{TcpListener, TcpStream};
#[tokio::main]
async fn main() {
    let listener = TcpListener::bind("0.0.0.0:8000")
        .await
        .expect("Failed to bind port!");

    loop {
        match listener.accept().await {
            Ok((stream, _)) => {
                tokio::spawn(handle_connection(stream));
            }
            Err(e) => {
                eprintln!("Error while establishing connection: {}", e);
            }
        }
    }
}

async fn handle_connection(mut stream: TcpStream) {
    // println!("Connection established!");
    let got_path = InputHandler::get_path(&mut stream).await;
    let status = Status::from_path_result(got_path);
    // println!("Respond status code: {:?}", status.code);
    OutputHandler::output(&mut stream, status).await;
}
