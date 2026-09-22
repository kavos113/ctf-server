export class SessionService {
  token = '';
  version = 0;
  notice = '';
  private listeners = new Set<() => void>();

  get authenticated() {
    return this.token.length > 0;
  }

  subscribe(listener: () => void) {
    this.listeners.add(listener);

    return () => {
      this.listeners.delete(listener);
    };
  }

  clear(notice = '') {
    this.token = '';
    this.notice = notice;
    this.version++;

    this.listeners.forEach((listener) => listener());
  }

  start(token: string) {
    this.clear();
    this.token = token;
  }
}
