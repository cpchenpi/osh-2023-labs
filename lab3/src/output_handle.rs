use std::{
    error::Error,
    fs::File,
    io::{BufReader, Read, Write},
    net::TcpStream,
};

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

fn write_all_u8(stream: &mut TcpStream, buf: &[u8]) -> Result<(), Box<dyn Error>> {
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
        let len = match status.code {
            StatusCode::Status200 => File::open(&status.path)?.metadata()?.len() as usize,
            _ => 0,
        };
        // println!("Output buf content: {:?}", buf);
        write_line(stream, &format!("Content-Length: {}", len))?;
        write_line(stream, "")?;
        if status.code == StatusCode::Status200 {
            let file = File::open(&status.path)?;
            let mut reader = BufReader::new(file);
            let mut read_len = 0;
            let mut buf = vec![0; 64 * 1024];
            while read_len < len {
                let single_len = reader.read(&mut buf[..])?;
                read_len += single_len;
                write_all_u8(stream, &buf[0..single_len])?;
            }
        }
        Ok(())
    }
}
