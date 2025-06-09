#!/usr/bin/env python3
import os
import re
import glob

def extract_info_from_file(filepath):
    """Extract required information from the last 100 lines of a .out file"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            # Read last 100 lines
            lines = f.readlines()
            last_lines = lines[-100:] if len(lines) > 100 else lines
            content = ''.join(last_lines)
        
        # Initialize extracted data
        extracted_data = {
            'instance_name': '',
            'ub': '',
            'lb': '',
            'elapsed_time': '',
            'nodes_explored': '',
            'branching_history': ''
        }
        
        # Extract instance name from the colored output line
        instance_match = re.search(r'<Instance:\s*([^>]+)>', content)
        if instance_match:
            extracted_data['instance_name'] = instance_match.group(1).strip()
        
        # Extract UB, LB, Elapsed Time, and Nodes Explored
        ub_match = re.search(r'<UB=\s*([^>]+)>', content)
        if ub_match:
            extracted_data['ub'] = ub_match.group(1).strip()
            
        lb_match = re.search(r'<LB=\s*([^>]+)>', content)
        if lb_match:
            extracted_data['lb'] = lb_match.group(1).strip()
            
        elapsed_match = re.search(r'<Elapsed Time=\s*([^>]+)>', content)
        if elapsed_match:
            extracted_data['elapsed_time'] = elapsed_match.group(1).strip()
            
        nodes_match = re.search(r'<Nodes Explored=\s*([^>]+)>', content)
        if nodes_match:
            extracted_data['nodes_explored'] = nodes_match.group(1).strip()
        
        # Extract branching_history.increase_depth
        branching_match = re.search(r'branching_history\.increase_depth:\s*(\[.*?\])', content, re.DOTALL)
        if branching_match:
            extracted_data['branching_history'] = branching_match.group(1).strip()
        
        return extracted_data
        
    except Exception as e:
        print(f"Error processing file {filepath}: {e}")
        return None

def generate_shortlog_filename(instance_name):
    """Generate shortlog filename from instance name"""
    # Extract numbers from instance name (e.g., CVRP_120_100 -> 120_100)
    numbers = re.findall(r'\d+', instance_name)
    if len(numbers) >= 2:
        return f"sLog_dg_{numbers[-2]}_{numbers[-1]}.txt"
    else:
        # Fallback if pattern doesn't match expected format
        clean_name = re.sub(r'[^\w\d_]', '_', instance_name)
        return f"sLog_{clean_name}.txt"

def main():
    # Get current directory
    input_folder = input("Enter the folder path containing .out files (or press Enter for current directory): ").strip()
    if not input_folder:
        input_folder = "."
    
    # Find all .out files
    out_files = glob.glob(os.path.join(input_folder, "*.out"))
    
    if not out_files:
        print(f"No .out files found in {input_folder}")
        return
    
    print(f"Found {len(out_files)} .out files")
    
    # Create output directory
    output_dir = "processed_logs"
    os.makedirs(output_dir, exist_ok=True)
    print(f"Created output directory: {output_dir}")
    
    # Process each file
    processed_count = 0
    for filepath in out_files:
        print(f"Processing: {os.path.basename(filepath)}")
        
        # Extract information
        data = extract_info_from_file(filepath)
        if data is None:
            continue
            
        # Generate output filename
        if data['instance_name']:
            output_filename = generate_shortlog_filename(data['instance_name'])
        else:
            # Fallback: use original filename
            base_name = os.path.splitext(os.path.basename(filepath))[0]
            output_filename = f"sLog_{base_name}.txt"
        
        output_path = os.path.join(output_dir, output_filename)
        
        # Write extracted information to file
        try:
            with open(output_path, 'w', encoding='utf-8') as f:
                f.write(f"Instance name: {data['instance_name']}\n")
                f.write(f"<UB= {data['ub']}> <LB= {data['lb']}> <Elapsed Time= {data['elapsed_time']}> <Nodes Explored= {data['nodes_explored']}>\n")
                f.write(f"branching_history.increase_depth: {data['branching_history']}\n")
            
            processed_count += 1
            print(f"  -> Created: {output_filename}")
            
        except Exception as e:
            print(f"  -> Error writing {output_filename}: {e}")
    
    print(f"\nProcessing complete! {processed_count} files processed successfully.")
    print(f"Output files saved in: {output_dir}/")

if __name__ == "__main__":
    main()