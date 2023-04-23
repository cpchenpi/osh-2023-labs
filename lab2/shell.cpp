// IO
#include <iostream>
// std::string
#include <string>
// C-type string
#include <cstring>
// std::vector
#include <vector>
// std::string 转 int
#include <sstream>
// PATH_MAX 等常量
#include <climits>
// POSIX API
#include <unistd.h>
// wait
#include <sys/wait.h>
// cmd queue
#include <queue>
// file
#include <fcntl.h>
// 字典
#include <unordered_map>

// 辅助函数
std::vector<std::string> split(std::string s, const std::string &delimiter);

std::queue<std::vector<std::string>> vstr_split(std::vector<std::string> &vstr,
                                                const std::string &delimiter,
                                                bool convert);

std::string join(std::vector<std::string> v, const std::string &delimiter);

bool safe_stoi(std::string s, int &x);

void process_pwd() {
    auto buf = get_current_dir_name();
    if (buf) {
        std::cout << buf << std::endl;
        free(buf);
    } else {
        int err = errno;
        switch (err) {
        case EACCES:
            std::cout << "Permission denied.\n";
            break;
        case ENOENT:
            std::cout << "The current working directory has been unlinked.\n";
            break;
        case ENOMEM:
            std::cout << "No enough memory!\n";
            break;
        default:
            std::cout << "Unknown error occured.\n";
            break;
        }
    }
}

void process_cd(std::vector<std::string> &args, char *home_dir) {
    if (args.size() > 2) {
        std::cout << "Too many arguments!\n";
        return;
    }

    const char *buf;
    std::string path;

    if (args.size() == 1) {
        // 根据bash的行为，参数为空时进入用户主目录
        buf = home_dir;
    } else {
        path = std::string{args[1]};
        if (args[1][0] == '~') {
            // 将~替换为主目录
            path.replace(0, 1, std::string{home_dir});
        }
        buf = path.data();
    }

    auto ret = chdir(buf);
    if (ret == -1) {
        int err = errno;
        if (err == EACCES) {
            std::cout << "Permission denied.\n";
        } else if (err == EFAULT) {
            std::cout << "Path points outside your accessible address "
                         "space.\n";
        } else if (err == EIO) {
            std::cout << "An I/O error occurred.\n";
        } else if (err == ELOOP) {
            std::cout << "Too many symbolic links were encountered in "
                         "resolving path.\n";
        } else if (err == ENAMETOOLONG) {
            std::cout << "Path is too long!\n";
        } else if (err == ENOENT) {
            std::cout << "Directory does not exist!\n";
        } else if (err == ENOMEM) {
            std::cout << "Insufficient kernel memory was available.\n";
        } else if (err == ENOTDIR) {
            std::cout << "A component of path is not a directory.\n";
        } else {
            std::cout << "Unknown error occurred.\n";
        }
    }
}

void process_one(std::vector<std::string> &args, int left_pipe[2],
                 int right_pipe[2]);

std::unordered_map<std::string, std::string> alias_map;

void process_out(std::queue<std::vector<std::string>> &cmds, int last_pipe[2]) {
    auto args = cmds.front();
    cmds.pop();
    if (!cmds.empty()) {
        int pipe_out[2];
        if (pipe(pipe_out) == -1) {
            std::cerr << "Pipe create failed!" << std::endl;
            return;
        }
        pid_t pid = fork();
        if (pid == 0) {
            process_one(args, last_pipe, pipe_out);
        } else {
            if (last_pipe) {
                close(last_pipe[0]);
                close(last_pipe[1]);
            }
            process_out(cmds, pipe_out);
        }
    } else {
        pid_t pid = fork();
        if (pid == 0) {
            process_one(args, last_pipe, 0);
        } else {
            if (last_pipe) {
                close(last_pipe[0]);
                close(last_pipe[1]);
            }
        }
    }
}

void process_one(std::vector<std::string> &args, int left_pipe[2],
                 int right_pipe[2]) {
    signal(SIGINT, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    if (args.size() >= 4) {
        int n = args.size();
        if ((args[n - 4] == "<" || args[n - 4] == "<<" ||
             args[n - 4] == "<<<") &&
            (args[n - 2] == ">" || args[n - 2] == ">>")) {
            std::swap(args[n - 4], args[n - 2]);
            std::swap(args[n - 3], args[n - 1]);
        }
    }
    if (left_pipe) {
        dup2(left_pipe[0], STDIN_FILENO);
        close(left_pipe[0]);
        close(left_pipe[1]);
    } else {
        if (args.size() >= 2) {
            int n = args.size();
            if (args[n - 2] == "<") {
                std::string filename = args.back();
                args.pop_back();
                args.pop_back();
                int file_in = open(filename.data(), O_RDONLY);
                if (file_in != -1) {
                    dup2(file_in, STDIN_FILENO);
                    close(file_in);
                } else {
                    std::cout << "Failed to open file " << filename << "!"
                              << std::endl;
                    return;
                }
            } else if (args[n - 2] == "<<") {
                std::string split_str = args.back();
                args.pop_back();
                args.pop_back();
                int tmp_file = open("./", O_TMPFILE | O_RDWR);
                if (tmp_file != -1) {
                    std::string line;
                    while (std::getline(std::cin, line), line != split_str) {
                        line += "\n";
                        write(tmp_file, line.data(), line.length());
                    }
                    lseek(tmp_file, SEEK_SET, 0);
                    dup2(tmp_file, STDIN_FILENO);
                    close(tmp_file);
                } else {
                    std::cout << "Failed to create temp file!" << std::endl;
                    return;
                }
            } else if (args[n - 2] == "<<<") {
                std::string temp_str = args.back();
                args.pop_back();
                args.pop_back();
                int tmp_file = open("./", O_TMPFILE | O_RDWR);
                if (tmp_file != -1) {
                    temp_str += "\n";
                    write(tmp_file, temp_str.data(), temp_str.length());
                    lseek(tmp_file, SEEK_SET, 0);
                    dup2(tmp_file, STDIN_FILENO);
                    close(tmp_file);
                } else {
                    std::cout << "Failed to create temp file!" << std::endl;
                    return;
                }
            }
        }
    }
    if (right_pipe) {
        dup2(right_pipe[1], STDOUT_FILENO);
        close(right_pipe[0]);
        close(right_pipe[1]);
    } else {
        if (args.size() >= 2) {
            int n = args.size();
            if (args[n - 2] == ">" || args[n - 2] == ">>") {
                std::string filename = args.back();
                args.pop_back();
                std::string op = args.back();
                args.pop_back();
                int file_in;
                if (op == ">")
                    file_in = open(filename.data(),
                                   O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU);
                else
                    file_in = open(filename.data(),
                                   O_WRONLY | O_APPEND | O_CREAT, S_IRWXU);
                if (file_in != -1) {
                    dup2(file_in, STDOUT_FILENO);
                    close(file_in);
                } else {
                    std::cout << "Failed to open/create file " << filename
                              << "!" << std::endl;
                    return;
                }
            }
        }
    }
    // std::vector<std::string> 转 char **
    char *arg_ptrs[args.size() + 1];
    for (auto i = 0; i < (int)args.size(); i++) {
        arg_ptrs[i] = (char *)malloc(args[i].length() + 1);
        auto buf = args[i].c_str();
        strcpy(arg_ptrs[i], buf);
    }

    // exec p 系列的 argv 需要以 nullptr 结尾
    arg_ptrs[args.size()] = nullptr;

    execvp(args[0].c_str(), arg_ptrs);
    // 之后这里的代码就没意义了 如果 execvp 之后的代码被运行了，那就是
    // execvp 出问题了
    exit(255);
}

pid_t main_pid;

bool handling = false;

void sigint_process(int sig) {
    if (!handling) {
        std::cout << std::endl << "# ";
        std::cout.flush();
    }
}

int main() {
    while (tcgetpgrp(0) != getpgid(getpid())) {
        // 等待进程成为前台进程
    }

    // 不同步 iostream 和 cstdio 的 buffer
    std::ios::sync_with_stdio(false);

    // 用来存储读入的一行命令
    std::string cmd;

    // 获取用户主目录
    char *home_dir = getenv("HOME");

    main_pid = getpid();

    signal(SIGINT, sigint_process);

    std::queue<pid_t> wait_queue;

    std::vector<std::string> history_cmds;

    while (true) {
        // 打印提示符
        std::cout << "# ";

        handling = false;

        // 读入一行。std::getline 结果不包含换行符。
        std::getline(std::cin, cmd);

        handling = true;

        // 按空格分割命令为单词
        std::vector<std::string> orig_args = split(cmd, " ");

        std::vector<std::string> args;

        bool failed = false, changed = false;

        for (auto s : orig_args) {
            if (s[0] == '!') {
                int n = history_cmds.size();
                if (s == "!!") {
                    s = "!" + std::to_string(n);
                }
                s.erase(0, 1);
                int x = 0;
                if (!safe_stoi(s, x)) {
                    std::cout << "Invalid argument: " << s << std::endl;
                    failed = true;
                    break;
                }
                if (x < 1 || x > n) {
                    std::cout << "No such command: " << s << std::endl;
                    failed = true;
                    break;
                }
                for (auto his_arg : split(history_cmds[x - 1], " ")) {
                    args.push_back(his_arg);
                }
                changed = true;
            } else {
                if (s.length() > 0 && s != " ")
                    args.push_back(s);
            }
        }

        if (failed)
            continue;

        if (changed) {
            std::cout << "Parsed as: " << join(args, " ") << std::endl;
        }

        history_cmds.push_back(join(args, " "));

        // 没有可处理的命令
        if (args.empty()) {
            continue;
        }

        if (args[0] == "history") {
            if (args.size() > 3) {
                std::cout << "Too many arguments!" << std::endl;
                continue;
            }
            if (args.size() == 1) {
                args.push_back(std::to_string(history_cmds.size()));
            }
            for (int i = std::stoi(args[1]); i >= 1; i--) {
                int pos = history_cmds.size() - i;
                if (pos >= 0) {
                    std::cout << "  " << pos + 1 << "  " << history_cmds[pos]
                              << std::endl;
                }
            }
        }

        if (args[0] == "alias") {
            if (args.size() == 1) {
                for (auto p : alias_map) {
                    std::cout << "alias " << p.first << " = " << p.second
                              << std::endl;
                }
                continue;
            } else {
                args.erase(args.begin());
                auto q = vstr_split(args, "=", false);
                if (q.size() >= 3) {
                    std::cout << "Too many arguments!" << std::endl;
                    continue;
                }
                std::string first, second;
                first = join(q.front(), " ");
                q.pop();
                if (q.size() == 0) {
                    if (alias_map.count(first)) {
                        std::cout << "alias " << first << " = "
                                  << alias_map[first] << std::endl;
                    } else {
                        std::cout << "alias " << first << " not found!"
                                  << std::endl;
                    }
                    continue;
                }
                second = join(q.front(), " ");
                alias_map[first] = second;
                std::cout << "alias " << first << " = " << alias_map[first]
                          << std::endl;
                continue;
            }
        }

        if (args[0] == "pids") {
            std::cout << getpid() << " " << getpgid(getpid()) << " "
                      << tcgetpgrp(0) << std::endl;
            continue;
        }

        // 退出
        if (args[0] == "exit") {
            if (args.size() <= 1) {
                return 0;
            }

            int code = 0;

            if (safe_stoi(args[1], code)) {
                return code;
            } else {
                std::cout << "Invalid exit code\n";
                continue;
            }
        }

        if (args[0] == "pwd") {
            process_pwd();
            continue;
        }

        if (args[0] == "cd") {
            process_cd(args, home_dir);
            continue;
        }

        if (args[0] == "wait") {
            if (args.size() > 1) {
                std::cout << "Too many arguments!" << std::endl;
                continue;
            }
            while (!wait_queue.empty()) {
                pid_t child_pid = wait_queue.front();
                waitpid(child_pid, nullptr, 0);
                std::cout << "Pid " << wait_queue.front() << " ends."
                          << std::endl;
                wait_queue.pop();
            }
            continue;
        }

        bool background = false;

        // 处理外部命令
        auto cmds = vstr_split(args, "|", true);

        if (cmds.back().back() == "&") {
            background = true;
            cmds.back().pop_back();
        }

        if (!cmds.empty()) {
            int n = cmds.size();
            pid_t pid = fork();
            if (pid == 0) {
                signal(SIGINT, SIG_IGN);
                signal(SIGTTIN, SIG_IGN);
                signal(SIGTTOU, SIG_IGN);
                signal(SIGCHLD, SIG_IGN);
                setpgid(0, 0);
                process_out(cmds, 0);
                for (int i = 0; i < n; i++) {
                    wait(nullptr);
                }
                if (!background)
                    tcsetpgrp(0, getpgid(main_pid));
                _exit(0);
            } else {
                if (!background) {
                    // 这里只有父进程（原进程）才会进入
                    while (getpgid(pid) != pid) {
                        // 等待子进程完成进程组的设置
                    }
                    tcsetpgrp(0, pid);
                    wait(&pid);
                } else {
                    std::cout << "Pid " << pid << " running" << std::endl;
                    wait_queue.push(pid);
                }
            }
        }
    }
}

std::vector<std::string> split(std::string s, const std::string &delimiter) {
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        res.push_back(token);
        s = s.substr(pos + delimiter.length());
    }
    res.push_back(s);
    return res;
}

std::queue<std::vector<std::string>> vstr_split(std::vector<std::string> &vstr,
                                                const std::string &delimiter,
                                                bool convert = true) {
    std::vector<std::vector<std::string>> ans;
    std::queue<std::vector<std::string>> res;
    int last_begin = 0, n = vstr.size();
    for (int i = 0; i < n; i++) {
        if (vstr[i] == delimiter) {
            if (i == last_begin || i == n - 1) {
                std::cout << "Wrong pipe format!" << std::endl;
                return res;
            }
            ans.emplace_back();
            for (int j = last_begin; j < i; j++) {
                ans.back().push_back(vstr[j]);
            }
            last_begin = i + 1;
        }
    }
    ans.emplace_back();
    for (int j = last_begin; j < n; j++) {
        ans.back().push_back(vstr[j]);
    }
    for (auto nargs : ans) {
        if (nargs.size() == 0)
            continue;
        std::vector<std::string> args;
        if (convert) {
            if (alias_map.count(nargs[0])) {
                for (auto s : split(alias_map[nargs[0]], " ")) {
                    args.push_back(s);
                }
            } else {
                args.push_back(nargs[0]);
            }
            for (int i = 1; i < (int)nargs.size(); i++) {
                args.push_back(nargs[i]);
            }
        } else {
            args = nargs;
        }
        res.push(args);
    }
    return res;
}

std::string join(std::vector<std::string> v, const std::string &delimiter) {
    int n = v.size();
    std::string ans;
    for (int i = 0; i < n; i++) {
        ans += v[i];
        if (i != n - 1) {
            ans += delimiter;
        }
    }
    return ans;
}

bool safe_stoi(std::string s, int &x) {
    std::stringstream stream(s);
    stream >> x;
    // 转换失败
    if (!stream.eof() || stream.fail()) {
        return false;
    }
    return true;
}