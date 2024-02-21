# Use a imagem oficial do Node.js LTS como base
FROM node:20.11.0-alpine

# Define o diretório de trabalho dentro do contêiner
WORKDIR /src

# Copia package.json e package-lock.json para o diretório de trabalho
COPY package*.json ./

# Instala as dependências
RUN npm install

# Executa o comando 'prisma generate' para gerar os artefatos do Prisma
RUN npx prisma generate

# Copia os arquivos compilados do diretório 'dist'
COPY . .

# Expondo a porta em que o aplicativo será executado
EXPOSE 3033

# Comando para executar sua aplicação TypeScript a partir dos arquivos compilados
CMD ["node", "dist/server.js"]
