export function createStore(scenario = 'normal') {
  const empty = scenario === 'empty';
  return {
    users: empty
      ? []
      : [
          { id: 'alice', username: 'alice', password: 'demo-password', score: 200 },
          { id: 'bob', username: 'bob', password: 'demo-password', score: 200 },
          { id: 'carol', username: 'carol', password: 'demo-password', score: 0 }
        ],
    challenges: empty
      ? []
      : [
          {
            id: 1,
            name: 'はじめてのフラグ',
            genre: 'web',
            description:
              'ようこそCTFへ。\nこの練習問題のフラグは flag{welcome} です。解答欄に入力してみましょう。',
            flag: 'flag{welcome}',
            creator_id: 'alice'
          },
          {
            id: 2,
            name: '文字の向こう側',
            genre: 'crypto',
            description:
              'この文字列を解読してください。\nZmxhZ3tiYXNlNjR9\n\n解法を考えるところから挑戦は始まります。',
            flag: 'flag{base64}',
            creator_id: 'bob'
          }
        ],
    answers: empty
      ? []
      : [
          {
            challenge_id: 1,
            user_id: 'bob',
            answer: 'wrong',
            correct: false,
            answered_at: '2026-09-22T00:00:00Z'
          },
          {
            challenge_id: 1,
            user_id: 'bob',
            answer: 'flag{welcome}',
            correct: true,
            answered_at: '2026-09-22T00:01:00Z'
          }
        ],
    sessions: new Map(),
    nextId: 3,
    nextUser: 1,
    nextToken: 1,
    nextAnswer: 2
  };
}
