import requests
from openai import OpenAI
import json
from models.models import ApiKey

class AIClient:
    @staticmethod
    def call_siliconflow_api(model, message, api_key, stream=False):
        """调用硅基流动API"""
        if stream:
            return AIClient.call_siliconflow_api_stream(model, message, api_key)
        else:
            return AIClient.call_siliconflow_api_sync(model, message, api_key)
    
    @staticmethod
    def call_siliconflow_api_sync(model, message, api_key):
        """非流式调用硅基流动API"""
        url = "https://api.siliconflow.cn/v1/chat/completions"
        
        payload = {
            "model": model,
            "messages": [{"role": "user", "content": message}],
            "max_tokens": 2048,
            "temperature": 0.7
        }
        
        headers = {
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json"
        }
        
        try:
            response = requests.post(url, json=payload, headers=headers, timeout=30)
            response.raise_for_status()
            
            result = response.json()
            if 'choices' in result and len(result['choices']) > 0:
                return result['choices'][0]['message']['content']
            else:
                return "抱歉，模型没有返回有效响应。"
                
        except requests.exceptions.RequestException as e:
            print(f"硅基流动API调用错误: {e}")
            return f"API调用失败: {str(e)}"
        except Exception as e:
            print(f"处理响应时出错: {e}")
            return f"处理响应时出错: {str(e)}"
    
    @staticmethod
    def call_siliconflow_api_stream(model, message, api_key):
        """流式调用硅基流动API"""
        try:
            client = OpenAI(
                base_url='https://api.siliconflow.cn/v1',
                api_key=api_key
            )
            
            return client.chat.completions.create(
                model=model,
                messages=[{"role": "user", "content": message}],
                stream=True,
                max_tokens=2048,
                temperature=0.7
            )
            
        except Exception as e:
            print(f"硅基流动流式API调用错误: {e}")
            return None