# OpenAPI E2E契約テスト

`../docs/openapi.yaml`だけをAPI仕様として、起動済みサーバーへHTTPリクエストを送信します。
clientと共通のTypeScript workspaceに属し、サーバー・DB・フロントエンドの実装は参照しません。

## 実行

Node.jsとpnpmのバージョンは[ルートの開発環境](../README.md)に従います。インストールはリポジトリルートで実行します。

```sh
pnpm install --frozen-lockfile
cd e2etest
pnpm check:api
pnpm typecheck
pnpm lint
pnpm fmt:check
pnpm test:unit
E2E_BASE_URL=http://localhost:8080 pnpm test
```

ルートからは`pnpm test`でclientと検証器のユニットテスト、
`E2E_BASE_URL=http://localhost:8080 pnpm test:e2e`でE2Eを実行できます。
共通依存のバージョンはルートのcatalog、TypeScript設定は`tsconfig.base.json`、fmt・lintはルートのOxfmt・Oxlint設定を使用します。
`pnpm fmt`で整形できます。生成型はfmt・lint対象外です。

`E2E_BASE_URL`は必須です。未指定・不正な場合、通信前に失敗します。
パス接頭辞が必要な環境では `http://localhost:8080/api` のように指定できます。
接頭辞を自動で補うことはありません。

接続先には破棄可能なテスト環境を指定してください。登録・問題作成・更新・削除・解答も送信します。
テストからサーバーやDBを起動・停止・初期化することはありません。
作成データは自動回収しません。環境の破棄・初期化は実行者が行ってください。

## 検証範囲

- 全12操作を、任意クエリの有無を含む14ケースで逐次実行します。操作の追加・削除によるケースとの不一致も検出します。
- 送信前に全ケースのボディとクエリをスキーマ検証します。文字列の識別子は実行ごとに生成し、独立ケースで指定する数値IDは `-1` とします。
- 受信ステータス、Content-Type、JSON構文、プロパティの型と日時などの形式を検証します。JSONのContent-Typeにはcharset指定を許容します。
- 仕様に定義された403・404も適合とします。すべてのステータス分岐を発生させるテストではありません。
- プロパティに必須指定がなければ欠落を許容し、追加プロパティも仕様で禁止されていなければ許容します。レスポンスボディが未定義の場合、ボディやContent-Typeには条件を追加しません。
- 認証ヘッダー・Cookieの保存や送信はしません。登録とログインには同じ生成済み入力を使用しますが、レスポンスのtokenやIDを他の操作に引き継ぎません。
- 得点、並び順、フラグ非公開、操作の成功、データの永続化など、スキーマが保証していない条件は検証しません。現実装との差は失敗として報告し、期待値を緩和しません。
- リクエストはボディ受信まで10秒でタイムアウトします。リダイレクト追従と再試行は行いません。

契約検証器は現在のOpenAPI 3.0スキーマで使われるJSON、クエリ、型・形式に対応します。
今後、別のメディアタイプやパラメーター位置などを仕様へ追加する際は、ケースと検証器の対応も更新してください。

## スキーマ変更時

```sh
pnpm generate:api
pnpm check:api
pnpm typecheck
pnpm test:unit
```

生成型は `src/generated/schema.ts` に保存します。手編集せず、OpenAPIから再生成してください。
実行時の期待値は毎回OpenAPIから読み込みます。型生成や検証のためにOpenAPIをコピー・改変しません。

`pnpm test:unit`はHTTP通信なしで、実際のOpenAPIを使って検証器の受理・拒否を確認します。
E2E失敗時には操作・実ステータス・検証箇所を表示します。接続失敗と契約違反は区別されます。
