import socket
import threading
import time
import random

# إعدادات الاتصال بالسيرفر
HOST = '127.0.0.1'
PORT = 6380
TOTAL_BACKGROUND_WRITES = 25000  # عدد العمليات الكلي لتوليد Compactions متكررة
CANARY_COUNT = 50                # عدد مفاتيح المراقبة الحساسة

def send_command(cmd):
    """دالة مساعدة لإرسال أمر واحد للسيرفر واستقبال الرد"""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((HOST, PORT))
            s.sendall(f"{cmd}\r\n".encode())
            return s.recv(1024).decode()
    except Exception as e:
        return f"-ERR {e}"

def background_load_generator():
    """خيط لتوليد ضغط كتابة عشوائي ومستمر لإجبار السيرفر على الـ Compaction التلقائي"""
    print("🚀 بدء خيط توليد الضغط الخلفي المتواصل...")
    for i in range(TOTAL_BACKGROUND_WRITES):
        key = f"load_key_{random.randint(1, 5000)}"
        val = f"val_{i}"
        send_command(f"SET {key} {val}")
    print("✅ انتهى خيط توليد الضغط الخلفي.")

def canary_chaos_worker(worker_id, expected_states):
    """خيط إحداث الفوضى: يرسل عمليات متناقضة وسريعة جداً على نفس المفاتيح"""
    print(f"🔥 بدء خيط الفوضى رقم {worker_id} لضرب بافر الطوارئ...")
    for i in range(CANARY_COUNT):
        key = f"canary_key_{worker_id}_{i}"
        
        # السيناريو الأخطر: كتابة ثم حذف ثم كتابة قيمة نهائية بسرعة خاطفة
        send_command(f"SET {key} initial_stale_value")
        send_command(f"DEL {key}")
        
        # تحديد عشوائي: هل ينتهي المفتاح بـ DEL أم بقيمة جديدة؟
        if random.choice([True, False]):
            send_command(f"SET {key} FINAL_VALID_VALUE")
            expected_states[key] = "FINAL_VALID_VALUE"
        else:
            send_command(f"DEL {key}")
            expected_states[key] = None  # متوقع أن يكون محذوفاً
            
        time.sleep(0.001) # تأخير ميكروسكوبي لزيادة احتمالية التداخل مع خيط الـ Compaction

def run_stress_test():
    expected_states = {}
    threads = []

    # 1. إطلاق خيط توليد الضغط المستمر
    load_thread = threading.Thread(target=background_load_generator)
    threads.append(load_thread)
    load_thread.start()

    # إعطاء الضغط ثانية واحدة ليبدأ السيرفر في العمل قبل إطلاق الفوضى
    time.sleep(0.5)

    # 2. إطلاق خيوط الفوضى المتزامنة لضرب البافر أثناء الـ Compaction
    for w in range(4): # 4 خيوط متزامنة
        t = threading.Thread(target=canary_chaos_worker, args=(w, expected_states))
        threads.append(t)
        t.start()

    # الانتظار حتى تنتهي جميع الخيوط تماماً
    for t in threads:
        t.join()

    print("\n--- 🏁 انتهاء مرحلة الضغط. بدء فحص سلامة البيانات الحالية ---")
    time.sleep(1) # انتظار ثانية لضمان إنهاء آخر خيط خلفي للـ Compaction في السيرفر

    mismatches = 0
    resurrections = 0

    # 3. الفحص والتحقق الصارم
    for key, expected_val in expected_states.items():
        res = send_command(f"GET {key}")
        
        if expected_val is None:
            # المفتاح يجب أن يكون محذوفاً
            if "Key Not Found" not in res:
                print(f"❌ خطأ كارثي (Resurrection Bug): المفتاح {key} يجب أن يكون محذوفاً، ولكن وجد قيمته: {res.strip()}")
                resurrections += 1
        else:
            # المفتاح يجب أن يحتوي على القيمة النهائية الصالحة
            expected_resp = f"${expected_val}"
            if expected_resp not in res:
                print(f"❌ خطأ تضارب (Stale Value Bug): المفتاح {key} قيمته المتوقعة {expected_val}، ولكن السيرفر أعاد: {res.strip()}")
                mismatches += 1

    print("\n--- 📊 التقرير النهائي للاختبار ---")
    if mismatches == 0 and resurrections == 0:
        print("🟢 معجزة هندسية! صمد النظام تماماً أمام 1000 طلب فوضوي متزامن مع الـ Compaction دون أي خطأ.")
    else:
        print(f"🔴 فشل الاختبار: تم رصد {mismatches} خطأ تضارب قيم، و {resurrections} حالة انبعاث لمفاتيح محذوفة.")

if __name__ == "__main__":
    run_stress_test()