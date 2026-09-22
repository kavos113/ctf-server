FROM node:24-bookworm-slim AS builder

WORKDIR /app
RUN npm install --global pnpm@12.5.1

COPY package.json pnpm-lock.yaml pnpm-workspace.yaml ./
COPY client/package.json ./client/package.json
COPY e2etest/package.json ./e2etest/package.json
RUN pnpm --filter client... install --frozen-lockfile

COPY tsconfig.base.json ./
COPY client ./client
RUN pnpm --dir client build

FROM nginx:stable-alpine

COPY client/nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=builder /app/client/dist /usr/share/nginx/html

EXPOSE 80
