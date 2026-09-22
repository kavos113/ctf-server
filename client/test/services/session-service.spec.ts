import { describe, it, expect, vi } from 'vitest';
import { SessionService } from '../../src/services/session-service';

describe('session', () => {
  it('start notifies subscribers after installing the new session', () => {
    const session = new SessionService();
    const observed: boolean[] = [];

    session.subscribe(() => observed.push(session.authenticated));
    session.start('first');
    const version = session.version;

    session.start('second');

    expect(observed).toEqual([true, true]);
    expect(session.token).toBe('second');
    expect(session.version).toBeGreaterThan(version);
    expect(session.notice).toBe('');
  });

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
