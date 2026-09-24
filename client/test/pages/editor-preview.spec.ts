import { describe, expect, it, vi } from 'vitest';
import { Registration } from 'aurelia';
import { createFixture } from '@aurelia/testing';
import { Api } from '../../src/api/api';
import { HttpClient } from '../../src/api/http-client';
import { SessionService } from '../../src/services/session-service';
import { EditorPage } from '../../src/pages/editor/editor-page';
import template from '../../src/pages/editor/editor-page.html';

describe('editor preview', () => {
  it('switches accessible tabs, preserves input and saves Markdown in create and edit modes', async () => {
    for (const editing of [false, true]) {
      const session = new SessionService();

      session.start('token');

      const api = new Api(new HttpClient(session));
      const input = {
        name: '問題',
        description: '**保存済み**',
        genre: 'web' as const,
        flag: 'flag'
      };

      vi.spyOn(api, 'myChallenges').mockResolvedValue([{ id: 1, ...input }]);

      let finish!: () => void;
      const save = vi.spyOn(api, 'saveChallenge').mockImplementation(
        () =>
          new Promise((resolve) => {
            finish = () => resolve({ id: 1 });
          })
      );
      const fixture = createFixture(template, EditorPage, [
        Registration.instance(Api, api),
        Registration.instance(SessionService, session)
      ]);

      await fixture.started;
      const page = fixture.component;

      await page.loading(editing ? { id: '1' } : {});

      if (!editing) {
        page.form = { ...input, description: '' };
      }

      await vi.waitFor(() => expect(fixture.appHost.querySelector('#description')).not.toBeNull());

      const edit = fixture.appHost.querySelector<HTMLButtonElement>('#description-tab-edit')!;
      const preview = fixture.appHost.querySelector<HTMLButtonElement>('#description-tab-preview')!;
      const textarea = fixture.appHost.querySelector<HTMLTextAreaElement>('#description')!;

      expect(edit.getAttribute('aria-selected')).toBe('true');
      preview.click();

      await vi.waitFor(() =>
        expect(fixture.appHost.querySelector('.description-preview')?.textContent).toContain(
          editing ? '保存済み' : '説明はありません。'
        )
      );
      expect(save).not.toHaveBeenCalled();

      edit.click();
      const source = '# 更新した説明\n<script>alert(1)</script>\n<img src="x" onerror="alert(1)">';

      textarea.value = source;
      textarea.dispatchEvent(new Event('input', { bubbles: true }));
      preview.click();

      await vi.waitFor(() =>
        expect(fixture.appHost.querySelector('.description-preview h1')?.textContent).toBe(
          '更新した説明'
        )
      );
      expect(
        fixture.appHost.querySelector('.description-preview script, .description-preview [onerror]')
      ).toBeNull();

      for (const [key, selected] of [
        ['Home', edit],
        ['ArrowLeft', preview],
        ['ArrowRight', edit],
        ['End', preview]
      ] as const) {
        selected.parentElement!.dispatchEvent(
          new KeyboardEvent('keydown', { key, bubbles: true, cancelable: true })
        );

        await vi.waitFor(() => expect(selected.getAttribute('aria-selected')).toBe('true'));
        expect(document.activeElement).toBe(selected);
        expect(selected.tabIndex).toBe(0);
      }

      expect(page.form.description).toBe(source);
      expect(save).not.toHaveBeenCalled();

      const saving = page.save();

      expect(save).toHaveBeenCalledWith({ ...input, description: source }, editing ? 1 : undefined);
      await vi.waitFor(() => expect(edit.matches(':disabled')).toBe(true));
      edit.click();
      page.selectDescriptionTab('edit');
      expect(page.descriptionTab).toBe('preview');

      finish();
      await saving;
      await page.loading({});

      expect(page.descriptionTab).toBe('edit');
      expect(page.form.description).toBeUndefined();

      page.form.description = 'private';
      page.selectDescriptionTab('preview');
      session.clear();

      expect(page.descriptionTab).toBe('edit');
      expect(page.form.description).toBeUndefined();
      expect(page.ready).toBe(false);
    }
  });
});
