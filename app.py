import serial
import json
import threading
import time
from flask import Flask, jsonify, render_template

app = Flask(__name__, template_folder='.')

# --- CONFIGURAÇÃO ---
# ALTERADO: Mudamos de 'COM3' para 'COM13' aqui
PORTA_ARDUINO = 'COM13'  
BAUD_RATE = 9600

# Dados iniciais
dados_remedios = {"p": 0, "b": 0} # P = Paracetamol, B = Buscopan

# Tenta conectar ao Arduino
try:
    arduino = serial.Serial(PORTA_ARDUINO, BAUD_RATE, timeout=1)
    print(f"Sucesso: Conectado na porta {PORTA_ARDUINO}")
except Exception as e:
    print(f"Erro: Não foi possível conectar na porta {PORTA_ARDUINO}")
    print(f"Detalhes: {e}")
    arduino = None

def ler_serial():
    """Lê a porta serial em background e atualiza a variável global"""
    global dados_remedios
    while True:
        if arduino and arduino.in_waiting > 0:
            try:
                linha = arduino.readline().decode('utf-8').strip()
                # Verifica se a linha parece um JSON válido
                if linha.startswith('{') and linha.endswith('}'):
                    novos_dados = json.loads(linha)
                    dados_remedios = novos_dados
                    print(f"Atualizado via Serial: {dados_remedios}")
            except Exception as e:
                # Erros de decodificação são comuns em Serial, apenas ignoramos
                print(f"Erro de leitura: {e}")
        time.sleep(0.1)

# Inicia a thread apenas se o Arduino foi conectado (para evitar erros)
if arduino:
    thread = threading.Thread(target=ler_serial)
    thread.daemon = True
    thread.start()
else:
    print("Aviso: Iniciando servidor sem conexão Serial (modo teste)")

@app.route('/')
def projeto():
    return render_template('projeto.html')

@app.route('/dados')
def dados():
    return jsonify(dados_remedios)

if __name__ == '__main__':
    # '0.0.0.0' permite acesso de outros dispositivos na rede local
    app.run(debug=True, host='0.0.0.0', port=5000)