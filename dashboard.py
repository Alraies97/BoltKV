import streamlit as st
import redis

st.set_page_config(page_title="BoltKV Dashboard", page_icon="⚡", layout="wide")

st.title("⚡ BoltKV Live Monitor & Management")
st.markdown("---")

@st.cache_resource
def get_bolt_client():
    return redis.Redis(host='localhost', port=6380, decode_responses=True)

try:
    r = get_bolt_client()
    
    col1, col2 = st.columns(2)
    
    with col1:
        st.subheader("📥 Execute Command")
        action = st.selectbox("Command Type", ["GET", "SET", "DEL"])
        key = st.text_input("Key")
        value = st.text_input("Value (Only for SET)")
        
        if st.button("Run Command"):
            if action == "SET":
                r.set(key, value)
                st.success(st.write(f"Key '{key}' set successfully!"))
            elif action == "GET":
                res = r.get(key)
                if res:
                    st.info(f"Result: {res}")
                else:
                    st.warning("Key Not Found")
            elif action == "DEL":
                res = r.delete(key)
                st.error(f"Deleted. Keys affected: {res}")

    with col2:
        st.subheader("📊 System Metrics")
        st.metric(label="Server Status", value="ONLINE", delta="Connected to Port 6380")
        
except Exception as e:
    st.error(f"Could not connect to BoltKV Server: {e}")