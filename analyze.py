import matplotlib.pyplot as plt
import matplotlib
import numpy as np
import os as os
import sys as sys

def ReadDiffSplitLog(dir_path):
    diff_split = {} 
    for log_name in os.listdir(dir_path):
        log_path = os.path.join(dir_path, log_name)
        with open(log_path, 'r') as log_file:
            content = log_file.readlines()[0]
            file_size = int(content.split(':')[2].split(' ')[1])
            file_size_str = log_name.split('_')[1]
            if any(c in file_size_str for c in ['GB', 'MB', 'KB']):
                file_size_str = file_size_str.replace('B', '')
            trans_time = float(content.split(':')[1].split(' ')[1])
            split_num = int(log_name.split('_')[2])
            if file_size_str not in diff_split.keys():
                diff_split[file_size_str] = {'slave':[], 'master':[]} 
            if log_name.find('s.log') != -1:
                diff_split[file_size_str]['slave'].append([file_size, file_size_str, trans_time, (file_size/trans_time)/1e3, split_num])
            elif log_name.find('m.log') != -1:
                diff_split[file_size_str]['master'].append([file_size, file_size_str, trans_time, (file_size/trans_time)/1e3, split_num])

    for key, value in diff_split.items():
        value['slave'].sort(key=lambda y:y[4])
        value['master'].sort(key=lambda y:y[4])

    return diff_split 

def ReadDiffBufLog(dir_path):
    diff_buf = {} 
    for log_name in os.listdir(dir_path):
        log_path = os.path.join(dir_path, log_name)
        with open(log_path, 'r') as log_file:
            content = log_file.readlines()[0]
            file_size = int(content.split(':')[2].split(' ')[1])
            file_size_str = log_name.split('_')[1] 
            if any(c in file_size_str for c in ['GB', 'MB', 'KB']):
                file_size_str = file_size_str.replace('B', '')
            trans_time = float(content.split(':')[1].split(' ')[1])
            buf_size = int(log_name.split('_')[3])
            if file_size_str not in diff_buf.keys():
                diff_buf[file_size_str] = {'slave':[], 'master':[]} 
            if log_name.find('s.log') != -1:
                diff_buf[file_size_str]['slave'].append([file_size, file_size_str, trans_time, (file_size/trans_time)/1e3, buf_size])
            elif log_name.find('m.log') != -1:
                diff_buf[file_size_str]['master'].append([file_size, file_size_str, trans_time, (file_size/trans_time)/1e3, buf_size])

    for key, value in diff_buf.items():
        value['slave'].sort(key=lambda y:y[4])
        value['master'].sort(key=lambda y:y[4])

    return diff_buf 

def ReadLog(dir_path):
    slave = []
    master = []
    for log_name in os.listdir(dir_path):
        log_path = os.path.join(dir_path, log_name)
        with open(log_path, 'r') as log_file:
            content = log_file.readlines()[0]
            file_size = int(content.split(':')[2].split(' ')[1])
            file_size_str = log_name.split('_')[1] 
            if any(c in file_size_str for c in ['GB', 'MB', 'KB']):
                file_size_str = file_size_str.replace('B', '')
            trans_time = float(content.split(':')[1].split(' ')[1])
            if log_name.find('s.log') != -1:
                slave.append([file_size, file_size_str, trans_time, (file_size/trans_time)/1e3])
            elif log_name.find('m.log'):
                master.append([file_size, file_size_str, trans_time, (file_size/trans_time)/1e3])

    master.sort(key=lambda y:y[0])
    slave.sort(key=lambda y:y[0])

    return {'slave':slave, 'master':master}

def SyncAndAsync(figure, sync, a_sync, title):
    x_slave = [data[1] for data in sync['slave']]
    y_slave_sync = [data[3] for data in sync['slave']]    
    y_slave_async = [data[3] for data in a_sync['slave']]    

    x_master = [data[1] for data in sync['master']]
    y_master_sync = [data[3] for data in sync['master']]    
    y_master_async = [data[3] for data in a_sync['master']]

    graph = plt.figure(figure)
    width = 0.4

    slave_xticks = np.arange(len(x_slave))
    slave = graph.add_subplot(311)
    slave.set_title(f'{title} slave async v.s. sync')
    slave.set_ylabel('bytes per sec')
    slave.set_xlabel('file size')
    slave.set_xticklabels(x_slave)
    slave.set_xticks(slave_xticks)
    slave.bar(slave_xticks - width/2, y_slave_sync, width, label='sync')
    slave.bar(slave_xticks + width/2, y_slave_async, width, label='async')
    slave.legend(loc='upper left')

    master_xticks = np.arange(len(x_master))
    master = graph.add_subplot(313)
    master.set_title(f'{title} master async v.s. sync')
    master.set_ylabel('bytes per sec')
    master.set_xlabel('file size')
    master.set_xticklabels(x_master)
    master.set_xticks(master_xticks)
    master.bar(master_xticks - width/2, y_master_sync, width, label='sync')
    master.bar(master_xticks + width/2, y_master_async, width, label='async')
    master.legend(loc='upper left')


def MmapAndFcntl(figure, mmap, fcntl, title):
    x_slave = [data[1] for data in mmap['slave']]
    y_slave_mmap = [data[3] for data in mmap['slave']]    
    y_slave_fcntl = [data[3] for data in fcntl['slave']]    

    x_master = [data[1] for data in mmap['master']]
    y_master_mmap = [data[3] for data in mmap['master']]    
    y_master_fcntl = [data[3] for data in fcntl['master']]

    graph = plt.figure(figure)
    width = 0.4

    slave_xticks = np.arange(len(x_slave))
    slave = graph.add_subplot(311)
    slave.set_title(f'{title} slave mmap v.s. fcntl')
    slave.set_ylabel('bytes per sec')
    slave.set_xlabel('file size')
    slave.set_xticklabels(x_slave)
    slave.set_xticks(slave_xticks)
    slave.bar(slave_xticks - width/2, y_slave_mmap, width, label='mmap')
    slave.bar(slave_xticks + width/2, y_slave_fcntl, width, label='fcntl')
    slave.legend(loc='upper left')

    master_xticks = np.arange(len(x_master))
    master = graph.add_subplot(313)
    master.set_title(f'{title} master mmap v.s. fcntl')
    master.set_ylabel('bytes per sec')
    master.set_xlabel('file size')
    master.set_xticklabels(x_master)
    master.set_xticks(master_xticks)
    master.bar(master_xticks - width/2, y_master_mmap, width, label='mmap')
    master.bar(master_xticks + width/2, y_master_fcntl, width, label='fcntl')
    master.legend(loc='upper left')

def DifferentBufSize(figure, fcntl, title):
    x_slave = [data[4] for data in fcntl['slave']]
    y_slave_buf = [data[3] for data in fcntl['slave']]    
    
    x_master = [data[4] for data in fcntl['master']]
    y_master_buf = [data[3] for data in fcntl['master']]

    graph = plt.figure(figure)
    width = 0.4

    slave_xticks = np.arange(len(x_slave))
    slave = graph.add_subplot(311)
    slave_list = fcntl['slave']
    slave.set_title(f'{title} slave with different buffer size')
    slave.set_ylabel('bytes per sec')
    slave.set_xlabel('buffer size in bytes')
    slave.set_xticklabels(x_slave)
    slave.set_xticks(slave_xticks)
    slave.bar(slave_xticks, y_slave_buf, width, label='fcntl')
    slave.legend(loc='upper left')

    master_xticks = np.arange(len(x_master))
    master = graph.add_subplot(313)
    master_list = fcntl['master']
    master.set_title(f'{title} master with different buffer size')
    master.set_ylabel('bytes per sec')
    master.set_xlabel('buffer size in bytes')
    master.set_xticklabels(x_master)
    master.set_xticks(master_xticks)
    master.bar(master_xticks, y_master_buf, width, label='fcntl')
    master.legend(loc='upper left')

def DifferentSplitNum(figure, fcntl, title):
    x_slave = [data[4] for data in fcntl['slave']]
    y_slave_buf = [data[3] for data in fcntl['slave']]    
    
    x_master = [data[4] for data in fcntl['master']]
    y_master_buf = [data[3] for data in fcntl['master']]

    graph = plt.figure(figure)
    width = 0.4

    slave_xticks = np.arange(len(x_slave))
    slave = graph.add_subplot(311)
    slave_list = fcntl['slave']
    slave.set_title(f'{title} slave splited into n files')
    slave.set_ylabel('bytes per sec')
    slave.set_xlabel('split into n files')
    slave.set_xticklabels(x_slave)
    slave.set_xticks(slave_xticks)
    slave.bar(slave_xticks, y_slave_buf, width, label='fcntl')
    slave.legend(loc='upper right')

    master_xticks = np.arange(len(x_master))
    master = graph.add_subplot(313)
    master_list = fcntl['master']
    master.set_title(f'{title} master splited into n files')
    master.set_ylabel('bytes per sec')
    master.set_xlabel('split into n files')
    master.set_xticklabels(x_master)
    master.set_xticks(master_xticks)
    master.bar(master_xticks, y_master_buf, width, label='fcntl')
    master.legend(loc='upper right')

if __name__ == '__main__':
    mmap_sync = ReadLog(sys.argv[1])
    mmap_async = ReadLog(sys.argv[2])

    fcntl_sync = ReadLog(sys.argv[3])
    fcntl_async = ReadLog(sys.argv[4])
    fcntl_buf = ReadDiffBufLog(sys.argv[5])
    fcntl_split = ReadDiffSplitLog(sys.argv[6])

    SyncAndAsync(figure=1, sync=mmap_sync, a_sync=mmap_async, title='mmap')
    SyncAndAsync(figure=2, sync=fcntl_sync, a_sync=fcntl_async, title='fcntl')

    MmapAndFcntl(figure=3, mmap=mmap_sync, fcntl=fcntl_sync, title='sync')
    MmapAndFcntl(figure=4, mmap=mmap_async, fcntl=fcntl_async, title='async')
        
    figure = 5
    for key, value in fcntl_buf.items():
        DifferentBufSize(figure=figure, fcntl=value, title=f'file size {key}')
        figure += 1

    for key, value in fcntl_split.items():
        DifferentSplitNum(figure=figure, fcntl=value, title=f'file size {key}')
        figure += 1

    plt.show()
