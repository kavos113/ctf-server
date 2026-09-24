# CTF client

Aurelia 2 + TypeScriptのCTFフロントエンドです。API契約は [`../docs/openapi.yaml`](../docs/openapi.yaml) を参照します。

DB・APIとまとめて起動する場合は、リポジトリルートで `docker compose up --build -d` を実行します。
クライアントは http://localhost:9000 で開けます。初期設定は[ルートの起動手順](../README.md)を参照してください。

## ソースの構成

`src/pages/` はページコンポーネントごとにディレクトリを分け、TypeScript・HTML・ページ固有のCSSを同じ場所に配置します。ログインと登録など、既存の同一コンポーネントを使う画面は引き続き共通化しています。

```text
src/
  pages/
    auth/
    challenges/
    detail/
    editor/
    history/
    my-challenges/
    not-found/
    ranking/
    shared/page-state.ts
  styles/
    app.css          # 共通CSSの読み込み口
    base.css         # 基本の色・文字・要素
    controls.css     # ボタン・フォーム
    components.css   # 共通のカード・表・状態表示
  my-app.css         # ヘッダー・ナビゲーション・全体レイアウト
```

ページ固有のCSSはページ名のクラスを起点に適用し、対応するTypeScriptから読み込みます。共通スタイルだけを使うページには空のCSSファイルを作りません。

ジャンルはサーバーの `ctf_genre` に合わせ、`src/api/genre.ts` の `Genre` にweb・crypto・pwn・rev・forensics・osint・miscを定義しています。作成・編集と一覧の絞り込みはこの共通定義を使用します。OpenAPIではgenreはstringのため、生成型を直接変更せず、フロントエンドの入力型で7種類に制限しています。

ジャンルの色は `src/styles/genres.css` の `:root` で変更できます。`--color-genre-web` が文字・枠線の色、`--color-genre-web-background` が背景色です。他のジャンルも同じ命名で、問題一覧・詳細・自分の問題一覧に共通で反映されます。

## 開発環境

問題のdescriptionはMarkdownとして表示します。見出し、強調、リスト、引用、コード、リンク、画像、表、取り消し線に対応し、通常の改行も反映します。入力・保存はMarkdown原文のままで、作成・編集画面の説明欄で「編集」「プレビュー」を切り替え、保存前の表示を確認できます。左右矢印・Home・Endキーでもタブを切り替えられます。表示時にmarkedで変換し、DOMPurifyで許可したタグ・属性だけを残してから挿入します。説明をAureliaのテンプレートとしてコンパイルしません。

Node.jsとpnpmのバージョンは[ルートの開発環境](../README.md)に従います。依存関係はルートのworkspace・catalog・lockfileで管理します。

```sh
pnpm install --frozen-lockfile
pnpm --filter client mock
```

別のターミナルで起動します。

```sh
cd client
pnpm start
```

`http://localhost:9000/#/challenges` を開きます。Viteは `/api/*` を既定で `http://127.0.0.1:8081/*` に転送します。モックはlocalhost限定で、再起動するとすべての状態が初期化されます。

- サンプルユーザー: `alice` / `bob` / `carol`
- パスワード（全員共通）: `demo-password`
- 問題1の練習用フラグ: `flag{welcome}`
- 問題2の練習用フラグ: `flag{base64}`

登録、ログイン、問題の作成・編集・削除、解答、履歴、ランキングを操作できます。自作問題・同じ問題への再提出も可能です。ログイン情報はメモリにのみ保持するため、再読み込み後は再ログインしてください。

## 実APIへの切替

コンテスト開始前に公開問題一覧を隠す場合は、`client/.env.local` にタイムゾーン付きの開始日時を指定してください。

```dotenv
VITE_CONTEST_START_AT=2026-10-01T10:00:00+09:00
```

開始前は一覧APIを呼ばず開始日時を表示し、問題詳細への直接アクセスも一覧へ戻します。画面を開いたままでも開始後に一覧を取得します。未指定なら従来どおり公開し、不正な日時の場合は非表示にします。作成者向けの自分の問題・編集画面は準備用として利用できます。

Viteの環境変数は開発サーバー起動時・本番ビルド時に反映されます。変更後は開発サーバーの再起動、または再ビルド・配信が必要です。この設定はブラウザー時刻による表示制御です。APIへの直接アクセスも開始前に禁止するには、サーバー側でも公開時刻を制御してください。

`client/.env.local` に以下を設定してViteを再起動します。

```dotenv
API_PROXY_TARGET=http://127.0.0.1:8080
```

clientから送る `/api` は開発proxyのための接頭辞で、OpenAPIのパスには含まれません。

認証はOpenAPIの `bearerAuth` に従い、ログインで取得したJWTを `Authorization: Bearer <token>` として送ります。解答送信・問題の変更・自分のデータ・ログアウトだけにJWTを付与し、公開APIには付与しません。JWTはメモリだけで保持し、署名・期限・セッションの検証はサーバーに任せます。再読み込み後は再ログインが必要です。

保護APIの401ではJWTと画面の非公開データを破棄し、ログインへ移動します。403ではログイン状態を維持します。ログアウトは提示したJWTのセッションだけを失効させます。200と401はログアウト完了として扱い、通信失敗や500では端末のJWTを破棄した上でサーバー側の失効を確認できなかった旨を表示します。古いリクエストの応答で新しいログイン状態を消さないようにしています。

登録・ログイン共通で、ユーザー名は半角英数字・ハイフン・アンダースコアの3〜32文字、パスワードはUTF-8で8〜128バイト（NUL禁止）です。入力をtrimせずそのまま送信し、503では時間を置いた再試行を案内します。ブラウザー側の検証とは別にサーバーが入力を再検証します。

## 型生成とチェック

[openapi-typescript CLI](https://openapi-ts.dev/cli) で入出力型・パス型を生成します。生成物は `src/api/generated/schema.ts` に保存してGit管理し、直接編集しません。OpenAPI上のoptionalはそのまま維持しています。

```sh
pnpm run generate:api
pnpm run check:api
pnpm run typecheck
pnpm run lint
pnpm run fmt:check
pnpm test
pnpm run build
```

JavaScript・TypeScriptのlintはoxlint、整形はoxfmtを使用します。設定はルートの[`.oxlintrc.json`](../.oxlintrc.json)・[`.oxfmtrc.json`](../.oxfmtrc.json)をE2Eと共有します。`client/`での`pnpm run fmt`は`src/`・`test/`・`mock/`と設定ファイルを整形し、`pnpm run fmt:check`で変更せずに確認できます。シングルクォート、2スペース、セミコロンありの書式を引き継ぎ、APIの生成ファイルはlint・整形から除外しています。CSSのlintはルートの[Stylelint設定](../.stylelintrc.json)を使用します。ルートでの同名コマンドはworkspace全体を対象にします。

設定の参照先: [Oxlintの設定](https://oxc.rs/docs/guide/usage/linter/config)、[Oxfmtの設定](https://oxc.rs/docs/guide/usage/formatter/config)。

自動テストはVitest + jsdomのユニットテストです。fetchをスタブに差し替え、モックのハンドラーも直接呼び出します。HTTPサーバーや実APIへの接続は不要です。ルーティングは既存の [Aureliaルーター](https://docs.aurelia.io/getting-to-know-aurelia/aurelia-router/fundamentals/getting-started) のハッシュモードです。

## モックのシナリオ

```sh
MOCK_SCENARIO=empty pnpm mock
MOCK_DELAY_MS=1500 pnpm mock
MOCK_SCENARIO=401 pnpm mock
```

| 設定 | 挙動 |
| --- | --- |
| `normal`（既定） | 状態を保持する通常の開発用API |
| `empty` | ユーザー・問題・解答を空で開始。新規登録可能 |
| `401` / `403` / `404` / `500` | 全操作が指定ステータスを返す |
| `non-json` | 200で不正なJSON本文を返す |
| `missing` | ログインtoken、公開問題ID、解答correct、ユーザーscoreを省略 |
| `scores-updated` | bobの表示scoreを350に変更 |
| `MOCK_DELAY_MS` | 応答遅延（ミリ秒）。他のシナリオと併用可能 |
| `MOCK_PORT` | ポートを変更（既定8081）。Viteのproxy先も変更する |

シナリオの切替はモックを再起動します。再起動後に古いtokenで保護操作を行うと401となり、再ログインが必要になります。モックの設定用エンドポイントはありません。

モック限定の仮定:

- 登録・ログインの入力制約とセッション単位の失効は実APIに合わせています。ただしモックのtokenは署名付きJWTではなく、署名や期限の検証は再現しません。
- 問題フォームの項目は空文字を許可しません。OpenAPIで必須・文字数制約が確定したら揃えます。
- 問題削除後も解答履歴は保持します。削除済み問題のIDで絞り込むAPIは404です。
- 採点ルールは未定義なので、解答による独自の加点処理は実装していません。`/users` のscoreはfixture値で、新規登録は仮に0点です。
- ランキングはscore降順、同点内はID順です。公式順位の仕様がないため順位番号を付けません。欠落したscoreは0点と区別します。

APIデータの永続キャッシュは持たず、画面を開くたびに取得します。解答後は履歴を再取得し、ランキングも移動時・更新ボタンで最新値を取得します。公開APIの応答からは許可フィールドのみを取り込み、問題フラグや他人の提出内容を公開画面の状態へ持ち込みません。

## 本番配信と残る確認

`pnpm run build` の `dist/` を静的配信し、同一オリジンの `/api/*` をAPIサーバーの `/*` へ転送してください。Viteの開発proxyは本番ビルドには含まれません。モックのコードや判定用フラグはブラウザーのエントリーポイントからimportしません。

自動テスト・型チェック・lint・ビルドに加え、リリース前にはブラウザーで画面幅、キーボード操作、直接遷移、再読み込みを確認してください。実サーバーとの接続、削除後の履歴、採点と公式順位の仕様は引き続き確認対象です。
