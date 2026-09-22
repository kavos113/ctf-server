const response = (status, body) => ({ status, body });

const project = (value, keys) =>
  Object.fromEntries(
    keys.filter((key) => value[key] !== undefined).map((key) => [key, value[key]])
  );

const publicChallenge = (value) =>
  project(value, ['id', 'name', 'description', 'genre', 'creator_id']);

const answerResponse = (store, value, publicView = false) => ({
  ...project(
    value,
    publicView
      ? ['challenge_id', 'answered_at']
      : ['challenge_id', 'answer', 'correct', 'answered_at']
  ),
  username: store.users.find((user) => user.id === value.user_id)?.username
});

const publicUser = (value) => project(value, ['id', 'username', 'score']);

const validStrings = (body, keys) =>
  body &&
  typeof body === 'object' &&
  keys.every((key) => typeof body[key] === 'string' && body[key].length > 0);

// In-memory request handler: no sockets, timers or global state, suitable for unit tests.
export function handleRequest(
  store,
  { method, path, query = {}, token, body },
  scenario = 'normal'
) {
  if (['401', '403', '404', '500'].includes(scenario)) return response(Number(scenario));

  if (scenario === 'non-json') return { status: 200, raw: 'not json' };

  const userId = store.sessions.get(token);
  const key = `${method} ${path}`;
  const protectedRoutes = new Set([
    'POST /logout',
    'POST /challenges',
    'PUT /challenges',
    'DELETE /challenges',
    'GET /challenges/me',
    'GET /answers/me',
    'POST /answers'
  ]);

  if (protectedRoutes.has(key) && !userId) return response(401);

  if (key === 'POST /signup' || key === 'POST /login') {
    if (
      !validStrings(body, ['username', 'password']) ||
      Object.keys(body).some((key) => !['username', 'password'].includes(key)) ||
      body.username.length < 3 ||
      body.username.length > 32 ||
      /[^A-Za-z0-9_-]/.test(body.username) ||
      body.password.includes('\0') ||
      new TextEncoder().encode(body.password).length < 8 ||
      new TextEncoder().encode(body.password).length > 128
    ) {
      return response(400);
    }
  }

  if (key === 'POST /signup') {
    if (!validStrings(body, ['username', 'password'])) return response(400);

    if (store.users.some((user) => user.username === body.username)) return response(409);

    const user = {
      id: `user-${store.nextUser++}`,
      username: body.username,
      password: body.password,
      score: 0
    };

    store.users.push(user);

    return response(201, project(user, ['id', 'username']));
  }

  if (key === 'POST /login') {
    const user = store.users.find(
      (user) => user.username === body?.username && user.password === body?.password
    );

    if (!user) return response(401);

    const token = `mock-token-${store.nextToken++}`;

    store.sessions.set(token, user.id);

    return response(200, scenario === 'missing' ? {} : { token });
  }

  if (key === 'POST /logout') {
    store.sessions.delete(token);

    return response(200);
  }

  if (key === 'GET /users') {
    return response(
      200,
      store.users.map((user) => {
        const value = publicUser(user);

        if (scenario === 'missing') delete value.score;

        if (scenario === 'scores-updated' && user.id === 'bob') value.score = 350;

        return value;
      })
    );
  }

  if (key === 'GET /challenges') {
    return response(
      200,
      store.challenges.map((item) => {
        const value = publicChallenge(item);

        if (scenario === 'missing') delete value.id;

        return value;
      })
    );
  }

  if (key === 'GET /challenges/me')
    return response(
      200,
      store.challenges.filter((item) => item.creator_id === userId).map((item) => ({ ...item }))
    );

  if (key === 'POST /challenges' || key === 'PUT /challenges') {
    let challenge;

    if (method === 'PUT') {
      if (query.id === undefined || !/^-?\d+$/.test(String(query.id))) return response(400);

      challenge = store.challenges.find((item) => item.id === Number(query.id));

      if (!challenge) return response(404);

      if (challenge.creator_id !== userId) return response(403);
    }

    if (!validStrings(body, ['name', 'description', 'genre', 'flag'])) return response(400);

    const input = project(body, ['name', 'description', 'genre', 'flag']);

    if (challenge) {
      Object.assign(challenge, input);

      return response(200, { ...challenge });
    }

    challenge = { ...input, id: store.nextId++, creator_id: userId };
    store.challenges.push(challenge);

    return response(201, { ...challenge });
  }

  if (key === 'DELETE /challenges') {
    if (query.id === undefined || !/^-?\d+$/.test(String(query.id))) return response(400);

    const challenge = store.challenges.find((item) => item.id === Number(query.id));

    if (!challenge) return response(404);

    if (challenge.creator_id !== userId) return response(403);

    store.challenges = store.challenges.filter((item) => item !== challenge);

    return response(200);
  }

  if (key === 'POST /answers') {
    if (!validStrings(body, ['answer']) || !Number.isInteger(body.challenge_id))
      return response(400);

    const challenge = store.challenges.find((item) => item.id === body.challenge_id);

    if (!challenge) return response(404);

    const answer = {
      challenge_id: challenge.id,
      user_id: userId,
      answer: body.answer,
      correct: body.answer === challenge.flag,
      answered_at: new Date(Date.UTC(2026, 8, 22, 0, store.nextAnswer++)).toISOString()
    };

    store.answers.push(answer);

    return response(
      200,
      scenario === 'missing' ? project(answer, ['challenge_id']) : answerResponse(store, answer)
    );
  }

  if (key === 'GET /answers' || key === 'GET /answers/me') {
    if (query.challenge_id !== undefined && !/^-?\d+$/.test(String(query.challenge_id)))
      return response(400);

    const id = query.challenge_id === undefined ? undefined : Number(query.challenge_id);

    if (id !== undefined && !store.challenges.some((item) => item.id === id)) return response(404);

    const answers = store.answers.filter(
      (item) =>
        (id === undefined || item.challenge_id === id) &&
        (path === '/answers/me' ? item.user_id === userId : item.correct)
    );

    return response(
      200,
      answers.map((item) => answerResponse(store, item, path !== '/answers/me'))
    );
  }

  return response(404);
}
