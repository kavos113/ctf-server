import { describe, it, expect, vi } from 'vitest';
import { SessionService } from '../../src/services/session-service';

describe('session', () => {
  it('clear invalidates requests and notifies active subscribers', () => {
    const session = new SessionService();
    const listener = vi.fn();
    session.start('token');
    const unsubscribe = session.subscribe(listener);
    const version = session.version;
    session.clear('expired');
    expect(session.authenticated).toBe(false);
    expect(session.version).toBeGreaterThan(version);
    expect(session.notice).toBe('expired');
    expect(listener).toHaveBeenCalledOnce();
    unsubscribe();
    session.clear();
    expect(listener).toHaveBeenCalledOnce();
  });
});
