# 🎧 PFL Preview — Monitoring Panel for OBS Studio

**PFL Preview** (Pre-Fader Listen) é um painel de monitoramento de áudio dedicado para o **Preview** do OBS Studio (Modo Estúdio / Studio Mode). 

Ele permite checar e ouvir qualquer fonte de áudio presente na cena do Preview (vídeos, mídias gravadas, áudio ao vivo, etc.) **sem enviar o áudio para o PGM (Program / Ar)**. Assim, você garante que o som está chegando no nível correto e funcionando perfeitamente antes de colocar a cena ao ar.

---

## ✨ Funcionalidades

- 🔊 **Monitoramento Pré-PGM:** Ouça o áudio da cena do Preview de forma 100% isolada, sem interferir no áudio da transmissão ao vivo.
- 🎬 **Suporte Completo a Fontes:** Funciona com vídeos, arquivos de áudio, entradas de microfone e mídias ao vivo.
- 🎛 **Seleção de Fonte de Áudio:** Escolha exatamente qual fonte de áudio presente na cena do Preview deseja isolar para escutar.
- 🎧 **Roteamento de Áudio:** Direciona o sinal para o dispositivo de monitoramento definido nas configurações globais de áudio do OBS.
- 📌 **Dock Integrado:** Painel acoplável diretamente na interface do OBS Studio.

---

## 🎯 Por que usar o PFL Preview?

No Modo Estúdio do OBS, ao carregar uma nova cena na janela de Preview contendo mídias ou entradas de áudio, não há uma forma nativa de ouvir esse som antes de realizar a transição para o PGM. 

O **pfl-preview** resolve essa limitação trazendo a função clássica de **PFL (Pre-Fader Listen / Solo)** das mesas de som profissionais diretamente para o OBS.

---

## 🖥️ Compatibilidade

| Sistema Operacional | Suportado | Notas |
| :--- | :---: | :--- |
| **Windows 10/11** (64-bit) | ✅ | Suporte exclusivo no momento |


> **Requisito de Versão:** Testado e otimizado para **OBS Studio 30.0+** em sistemas 64-bit.

---

## 📦 Como Instalar (Windows)

1. Acesse a seção de **[Releases](https://github.com/dablofilmes/pfl-preview/releases)** e baixe a versão mais recente em formato `.zip`.
2. Extraia o conteúdo do arquivo `.zip`.
3. Copie as pastas extraídas (`obs-plugins` e `data`) e cole na pasta raiz do seu OBS Studio:
   - Caminho padrão: `C:\Program Files\obs-studio\`
4. Se solicitado pelo Windows, confirme a mesclagem das pastas.
5. Reinicie o OBS Studio.

---

## 🚀 Como Usar

1. Abra o OBS Studio e ative o **Modo Estúdio** (Studio Mode).
2. Vá ao menu superior do OBS: **Docks** ➔ **PFL Preview**.
3. Encaixe o painel na interface do OBS onde for mais conveniente.
4. Selecione a cena desejada na janela de **Preview**.
5. No painel do **PFL Preview**, selecione a fonte de áudio que deseja monitorar.
6. O áudio será reproduzido no seu dispositivo de monitoramento configurado no OBS, sem ir para a transmissão (PGM).

---

## 📄 Licença

Este projeto está licenciado sob a licença [GPL-2.0](LICENSE) — consulte o arquivo de licença para mais detalhes.

---

## 🤝 Contribuições & Suporte

- Encontrou um bug ou tem uma sugestão de nova funcionalidade? Abra uma **[Issue](https://github.com/usuario/pfl-preview/issues)**.
- Se o **pfl-preview** te ajudou nas suas transmissões, considere deixar uma ⭐️ no repositório!
