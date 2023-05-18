use std::error::Error;

use tokio::{
    fs::File,
    io::{AsyncReadExt, AsyncWriteExt, BufReader},
    net::TcpStream,
};

use crate::status::{Status, StatusCode};

pub struct OutputHandler;

async fn write_line(stream: &mut TcpStream, s: &str) -> Result<(), Box<dyn Error>> {
    stream.write_all(&format!("{}\r\n", s).into_bytes()).await?;
    Ok(())
}

impl OutputHandler {
    pub async fn output(stream: &mut TcpStream, status: Status) {
        if let Err(e) = Self::handle(stream, status).await {
            eprintln!("Error while handling output: {}", e);
        }
    }

    async fn handle(stream: &mut TcpStream, status: Status) -> Result<(), Box<dyn Error>> {
        let status_line = status.get_status_line();
        write_line(stream, status_line).await?;
        let len = match status.code {
            StatusCode::Status200 => {
                File::open(&status.path).await?.metadata().await?.len() as usize
            }
            _ => 0,
        };
        // println!("Output buf content: {:?}", buf);
        write_line(stream, &format!("Content-Length: {}", len)).await?;
        write_line(stream, "").await?;
        if status.code == StatusCode::Status200 {
            let file = File::open(&status.path).await?;
            let mut reader = BufReader::new(file);
            let mut read_len = 0;
            let mut buf = vec![0; 32 * 1024];
            while read_len < len {
                let single_len = reader.read(&mut buf[..]).await?;
                read_len += single_len;
                stream.write_all(&mut buf[0..single_len]).await?;
            }
        }
        Ok(())
    }
}
