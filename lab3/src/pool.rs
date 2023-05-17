use std::{
    sync::{
        mpsc::{self, Sender},
        Arc, Mutex,
    },
    thread::{self},
};

type Job = Box<dyn FnOnce() + Send + 'static>;

pub struct ThreadPool {
    sender: Sender<Job>,
}

impl ThreadPool {
    pub fn new(n: usize) -> Self {
        let (sender, receiver) = mpsc::channel();

        let receiver = Arc::new(Mutex::new(receiver));

        for _ in 0..n {
            ThreadWrapper::new(Arc::clone(&receiver));
        }

        ThreadPool { sender }
    }

    pub fn send<T>(&self, job: T)
    where
        T: FnOnce() + Send + 'static,
    {
        let job = Box::new(job);
        self.sender.send(job).expect("Failed to send one job");
    }
}

struct ThreadWrapper;

impl ThreadWrapper {
    pub fn new(receiver: Arc<Mutex<mpsc::Receiver<Job>>>) -> Self {
        thread::spawn(move || loop {
            let job = receiver
                .lock()
                .expect("Mutex is poisoned!")
                .recv()
                .expect("Failed to get job!");
            // println!("Thread {:?} got a job.", thread::current().id());
            job();
            // println!("Thread {:?} finished a job", thread::current().id());
        });

        ThreadWrapper {}
    }
}
