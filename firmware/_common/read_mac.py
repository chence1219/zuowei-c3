import re
import time
import serial
import serial.tools.list_ports
from threading import Thread, Event, Lock, current_thread  # ✅ 修正导入
import sys
import os


# ===================================================================================
# 配置区域
# ===================================================================================

MAC_PATTERN = re.compile(
    r'sta\s*\(\s*([0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2})\)',
    re.IGNORECASE
)

MAC_FILE = 'mac.txt'
BAUDRATES = [115200, 74880, 9600, 57600, 4800]
SCAN_INTERVAL = 0.8

stop_event = Event()
saved_macs = set()
file_lock = Lock()
threads_lock = Lock()
active_threads = {}
active_serials = {}


# ===================================================================================
# 文件操作
# ===================================================================================

def load_existing_macs():
    if os.path.exists(MAC_FILE):
        try:
            with open(MAC_FILE, 'r', encoding='utf-8') as f:
                for line in f:
                    mac = line.strip().lower()
                    if mac:
                        saved_macs.add(mac)
            print(f"📂 已加载 {len(saved_macs)} 个历史 MAC 地址")
        except Exception as e:
            print(f"❌ 无法读取 {MAC_FILE}: {e}")
    else:
        print(f"📝 {MAC_FILE} 不存在，将创建新文件")


def save_mac_address(mac: str):
    mac = mac.lower()
    with file_lock:
        if mac not in saved_macs:
            try:
                with open(MAC_FILE, 'a', encoding='utf-8') as f:
                    f.write(mac + '\n')
                print(f"🎉 新 MAC 地址已保存: {mac}")
                saved_macs.add(mac)
            except Exception as e:
                print(f"❌ 写入文件失败: {e}")


# ===================================================================================
# 串口管理
# ===================================================================================

def find_all_tty():
    ports = serial.tools.list_ports.comports()
    result = []
    for port in ports:
        if "Bluetooth" in port.description:
            continue
        if "COM" in port.device or "tty" in port.device:
            result.append(port.device)
    return result


def try_connect(port, baudrate):
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        return ser
    except Exception:
        return None


# ===================================================================================
# 监听单个串口
# ===================================================================================

def monitor_serial_port(port):
    if port in active_serials or port in active_threads:
        return

    with threads_lock:
        if port in active_threads:
            return
        active_threads[port] = current_thread()  # ✅ 修复完成

    ser = None
    for baudrate in BAUDRATES:
        if stop_event.is_set():
            return
        ser = try_connect(port, baudrate)
        if ser:
            break

    if not ser:
        print(f"❌ 无法连接 {port} @ 常见波特率")
        with threads_lock:
            active_threads.pop(port, None)
        return

    with threads_lock:
        active_serials[port] = ser

    print(f"✅ {port} 已连接 @ {ser.baudrate} bps，开始监听...")

    while not stop_event.is_set():
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    match = MAC_PATTERN.search(line)
                    if match:
                        mac = match.group(1).lower()
                        save_mac_address(mac)
            else:
                time.sleep(0.05)
        except (serial.SerialException, OSError):
            break
        except Exception as e:
            print(f"❌ {port} 读取异常: {e}")
            break

    try:
        ser.close()
    except:
        pass

    with threads_lock:
        active_serials.pop(port, None)
        active_threads.pop(port, None)

    if not stop_event.is_set():
        print(f"🔌 {port} 已断开，等待重新插入...")


# ===================================================================================
# 插拔检测
# ===================================================================================

def detect_port_changes():
    known_ports = set()
    while not stop_event.is_set():
        try:
            current_ports = set(find_all_tty())
            new_ports = current_ports - known_ports
            for port in new_ports:
                print(f"🆕 检测到新串口设备插入: {port}")
                thread = Thread(target=monitor_serial_port, args=(port,), daemon=True)
                thread.start()
            known_ports = current_ports
            time.sleep(SCAN_INTERVAL)
        except Exception as e:
            print(f"❌ 设备扫描异常: {e}")
            time.sleep(1)


# ===================================================================================
# 主函数
# ===================================================================================

def main():
    print("🚀 多线程串口 MAC 地址监听器启动...")
    print(f"💾 MAC 地址将保存到: {MAC_FILE}")
    load_existing_macs()

    detector_thread = Thread(target=detect_port_changes, daemon=True)
    detector_thread.start()

    print("🔍 正在监听串口设备插拔...")

    try:
        while not stop_event.is_set():
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n👋 正在关闭程序...")

    stop_event.set()

    with threads_lock:
        threads_to_join = list(active_threads.values())
    for t in threads_to_join:
        t.join(timeout=0.5)

    print("✅ 所有监听线程已安全退出，程序结束")


if __name__ == "__main__":
    main()