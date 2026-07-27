#!/usr/bin/env python3
import argparse
import subprocess
import os
import sys
import socket
import shutil

def get_ota_tool():
    mac_path = os.path.expanduser("~/Library/Arduino15")
    print(f"[DEBUG] Checking tool base folder: {mac_path}")
    if not os.path.exists(mac_path):
        print(f"[DEBUG] Folder {mac_path} does not exist!")
        return None
    try:
        cmd = f"find {mac_path} -name 'espota.py' | head -n 1"
        print(f"[DEBUG] Finding tool via: {cmd}")
        tool = subprocess.check_output(cmd, shell=True, stderr=subprocess.DEVNULL).decode().strip()
        print(f"[DEBUG] Found ota tool path: {tool}")
        return tool if tool else None
    except Exception as e:
        print(f"[DEBUG] Exception finding tool: {e}")
        return None

def resolve_ip(hostname):
    try:
        cmd = f"ping -c 1 {hostname}.local | head -n 1 | awk -F'[()]' '{{print $2}}'"
        print(f"[DEBUG] Running IP resolution: {cmd}")
        ip = subprocess.check_output(cmd, shell=True, stderr=subprocess.DEVNULL).decode().strip()
        print(f"[DEBUG] Hostname {hostname}.local mapped to IP: {ip}")
        return ip if ip else None
    except Exception as e:
        print(f"[DEBUG] Exception during host ping resolve: {e}")
        return None

def get_mac_local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        print(f"[DEBUG] Mac Local Network IP discovered: {local_ip}")
        return local_ip
    except Exception as e:
        print(f"[DEBUG] Exception fetching link-local IP: {e}")
        sys.exit(1)

def run_ota(name, ip, bin_file, tool, password):
    print(f"[!] Pushing firmware to {name} at {ip}...")
    local_mac_ip = get_mac_local_ip()
    
    cmd = [
        "python3", tool, 
        "-i", ip, 
        "-I", local_mac_ip, 
        "-p", "3232", 
        "-a", password,
        "-f", bin_file
    ]
    print(f"[DEBUG] Full Executable OTA Array: {cmd}")
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    output = result.stdout + result.stderr
    
    print(f"[DEBUG] Tool return exit code: {result.returncode}")
    if not result.returncode:
        print(f"[✔] {name}: Update Successful.")
        return True
    else:
        print(f"[❌] {name}: Update Failed!")
        print("\n--- ESPOTA OUTPUT TRACE ---")
        print(output)
        print("---------------------------")
        return False

def main():
    source_build_bin = "./build/esp32.esp32.m5stack_tough/m5Tough.ino.bin"
    target_staging_dir = "./bin"
    default_bin = os.path.join(target_staging_dir, "m5Tough.ino.bin")
    
    parser = argparse.ArgumentParser(description="SmartPoolFiller Flash Push Script")
    parser.add_argument("-b", "--bin", default=default_bin, help=f"Path to .bin file (Default: {default_bin})")
    parser.add_argument("-s", "--target", help="Optional single target hostname (e.g. SmartPoolFiller-Tough-2)")
    parser.add_argument("-a", "--auth", default="admin", help="OTA password string (Default: admin)")
    args = parser.parse_args()

    print(f"[DEBUG] CLI Parsed Args -> bin: {args.bin}, target: {args.target}, auth: {args.auth}")

    # Staging Phase Diagnostics
    if args.bin == default_bin:
        print(f"[DEBUG] Staging block triggered. Checking: {source_build_bin}")
        if os.path.exists(source_build_bin):
            print(f"[DEBUG] Source file exists. Making directory: {target_staging_dir}")
            os.makedirs(target_staging_dir, exist_ok=True)
            try:
                shutil.copy2(source_build_bin, default_bin)
                print(f"[✔] Staged successfully: {source_build_bin} -> {default_bin}")
            except Exception as e:
                print(f"[DEBUG] Failed to copy build artifact: {e}")
        else:
            print(f"[DEBUG] Build artifact missing from disk path: {source_build_bin}")

    ota_tool = get_ota_tool()
    if not ota_tool:
        print("[!] Could not locate espota.py inside ~/Library/Arduino15 directories.")
        sys.exit(1)
        
    print(f"[DEBUG] Verifying actual bin presence at: {args.bin}")
    if not os.path.exists(args.bin):
        print(f"[!] Target binary file missing or unreachable: {args.bin}")
        sys.exit(1)
    else:
        print(f"[DEBUG] Binary verification passed.")

    # 1. Single Target Mode
    if args.target:
        print(f"[*] Focused Target Destination: {args.target}")
        ip = resolve_ip(args.target)
        if not ip:
            print(f"[!] Could not locate {args.target}.local on your router subnet.")
            sys.exit(1)
        run_ota(args.target, ip, args.bin, ota_tool, args.auth)

    # 2. Fleet Discovery Mode
    else:
        print("[*] Scanning for all active pool frames (2 second snapshot)...")
        try:
            cmd = "dns-sd -B _arduino._tcp local & sleep 2; kill $!"
            print(f"[DEBUG] Discovery Callout Execution: {cmd}")
            raw_list = subprocess.check_output(cmd, shell=True, stderr=subprocess.DEVNULL).decode()
            print(f"[DEBUG] Raw Discovery Output:\n{raw_list}")
            
            # FIXED: Safely parsing individual string items at element index 6 to protect set() tracking
            names = []
            for line in raw_list.splitlines():
                if "SmartPoolFiller" in line:
                    parts = line.split()
                    if len(parts) > 6:
                        names.append(parts[6])
                        
            print(f"[DEBUG] Parsed targets array before filter: {names}")
            unique_targets = sorted(list(set(names)))
            print(f"[DEBUG] Filtered clean targets: {unique_targets}")
            
            if not unique_targets:
                print("[!] No SmartPoolFiller panels found broadcasting over _arduino._tcp.")
                sys.exit(1)
                
            print(f"[✔] Discovered {len(unique_targets)} active hardware panel modules.")
            for name in unique_targets:
                print(f"[*] Resolving IP for {name}...")
                ip = resolve_ip(name)
                if ip:
                    run_ota(name, ip, args.bin, ota_tool, args.auth)
                else:
                    print(f"[!] Could not resolve network IP endpoint for host: {name}")
                    
        except Exception as e:
            print(f"[!] Network scan pipeline encounter block: {e}")
            import traceback
            traceback.print_exc()
            
    print("[*] Deployment script run complete.")

if __name__ == "__main__":
    main()
