import asyncio
import time

SERVER_IP = '127.0.0.1'
PORT = 6380
NUM_WORKERS = 4
REQUESTS_PER_WORKER = 5000  
async def launch_worker(worker_id):
    print(f"[Worker {worker_id}] Starting...")
    try:
        reader, writer = await asyncio.open_connection(SERVER_IP, PORT)
    except Exception as e:
        print(f"[Worker {worker_id}] Connection failed: {e}")
        return 0

    start_time = time.time()
    
    for i in range(REQUESTS_PER_WORKER):
        cmd = f"SET worker_{worker_id}_key_{i} value_{i}\r\n"
        writer.write(cmd.encode())
        await writer.drain()
        
        response = await reader.readline()
        if not response.startswith(b'+OK'):
            print(f"[Worker {worker_id}] Unexpected response: {response}")
            
    writer.close()
    await writer.wait_closed()
    
    duration = time.time() - start_time
    print(f"[Worker {worker_id}] Finished in {duration:.2f}s")
    return REQUESTS_PER_WORKER

async def main():
    print(f"🚀 Starting Concurrent Benchmark with {NUM_WORKERS} workers...")
    start_total = time.time()
    
    tasks = [launch_worker(w_id) for w_id in range(NUM_WORKERS)]
    total_requests = sum(await asyncio.gather(*tasks))
    
    total_duration = time.time() - start_total
    qps = total_requests / total_duration
    avg_latency = (total_duration / total_requests) * 1000
    
    print("\n" + "="*40)
    print(f"📊 BENCHMARK RESULTS:")
    print(f"🔹 Total Requests Processed: {total_requests}")
    print(f"🔹 Total Time Taken:         {total_duration:.2f} seconds")
    print(f"🔹 Throughput (QPS):         {int(qps)} req/sec")
    print(f"🔹 Avg Latency per Request:  {avg_latency:.4f} ms")
    print("="*40)

if __name__ == '__main__':
    asyncio.run(main())