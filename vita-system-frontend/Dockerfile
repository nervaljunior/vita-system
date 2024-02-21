# Use uma imagem do Node.js como base
FROM node:20.11.0-alpine

# Defina o diretório de trabalho dentro do contêiner
WORKDIR /app

# Copie os arquivos necessários para o contêiner
COPY package.json .
COPY package-lock.json .
COPY .env.local .  

# Instale as dependências
RUN npm install

# Copie o restante dos arquivos para o contêiner
COPY . .

# Construa o aplicativo Next.js para produção
RUN npm run build

# Exponha a porta 3000
EXPOSE 3000

# Comando para iniciar o servidor Next.js
CMD ["npm", "start"]
