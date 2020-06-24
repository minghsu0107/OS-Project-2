# OS-project-2
Goal: Transfer multiple files from a place to another through kernel sockets, using read()/write() or mmap(). 

We also experiment with different file sizes to analyze the performance. Please refer to `Report.md` for detailed explanations.

Kernel Version: `5.3.0-28-generic`
### Usage
Compile the code:
```bash
./compile.sh
```
Send a file with fcntl:
```
./user_program/master 1 ./data/file1_in fcntl
```
Receive a file with fcntl and save it to `./data/file1_out`:
```
./user_program/slave 1 ./data/file1_out fcntl 127.0.0.1
```
Clean up all files:
```bash
./clean.sh
```

### Evaluation
`eval.sh` will generate logs for file transmissions.
#### Usage
```bash
./eval.sh [mode] [master log] [slave log] [file1 file2 file3...]   
```
#### Example:
```bash
./eval.sh fcntl ./log/master.log ./log/slave.log ./data/file1_in ./data/file2_in
```
- The output files will be stored at `./data/[file_name]_out`.

### Experiment of Splitting Files
#### Usage
```bash
./fcntl_run.sh
./mmap_run.sh
```
- The log file will be stored at `./log/fcntl` or `./log/mmap` by default.
- The format of file name: `[mode]_[file_size]_[file_num]_[master/slave].log`.

### Plot Graph
#### Usage
```bash
python3 analyze.py mmap_sync_dir mmap_async_dir fcntl_sync_dir fcntl_async_dir fcntl_diff_buf_dir fcntl_diff_split_dir
```
- dependency
  - numpy
  - matplotlib
