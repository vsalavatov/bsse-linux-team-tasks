import unittest
import os

current_directory = ""


class TestBablib(unittest.TestCase):

    @classmethod
    def setUp(self):
        os.chdir(current_directory + '/tmp')

    @staticmethod
    def get_ls_output(d=''):
        stream = os.popen(f'ls {d}')
        output = stream.read()
        actual = set(output.split())
        stream.close()
        return actual

    @staticmethod
    def get_contents(path):
        with open(path, 'r') as f:
            return f.read()

    @staticmethod
    def move(f, t):
        stream = os.popen(f'mv {f} {t}')
        output = stream.read()
        actual = set(output.split())
        stream.close()
        return actual

    def test_structure(self):
        # 2 rooms, 4 bookcases and desk in root room of the library
        self.assertSetEqual(self.get_ls_output(), {'bookcase_0', 'bookcase_1', 'bookcase_2', 'bookcase_3', 'room_9', 'room_1', 'desk'})

        # 1 room, 4 bookcases and desk in the room, which we came to from another room
        os.chdir('room_1')
        self.assertSetEqual(self.get_ls_output(), {'bookcase_0', 'bookcase_1', 'bookcase_2', 'bookcase_3', 'room_2', 'desk'})

        # 5 shelves in bookcase
        os.chdir('bookcase_2')
        self.assertSetEqual(self.get_ls_output(), {'shelf_0', 'shelf_1', 'shelf_2', 'shelf_3', 'shelf_4'})

        # 32 books in shelf
        os.chdir('shelf_3')
        ls_output = self.get_ls_output()
        self.assertEqual(len(ls_output), 32)
        self.assertEqual(len(list(filter(lambda s: s.startswith('book_'), ls_output))), 32)

    def test_cycle(self):
        os.chdir('room_1')
        contents_before = 'I was here'
        with open('desk/note', 'w') as f:
            print(contents_before, file=f, end='')
        ls_before = self.get_ls_output()
        for i in range(10):
            os.chdir(f'room_{(i + 2) % 10}')
        ls_after = self.get_ls_output()
        with open('desk/note', 'r') as f:
            contents_after = f.read()
        self.assertEqual(contents_before, contents_after)
        self.assertEqual(ls_before, ls_after)
        
    def test_room_renaming_forbidden(self):
        self.move('room_1', 'kek')
        self.assertSetEqual(self.get_ls_output(), {'bookcase_0', 'bookcase_1', 'bookcase_2', 'bookcase_3', 'room_9', 'room_1', 'desk'})

    def test_bookcase_renaming_ok(self):
        os.chdir('room_1')
        self.move('bookcase_0', 'kek')
        self.assertSetEqual(self.get_ls_output(), {'kek', 'bookcase_1', 'bookcase_2', 'bookcase_3', 'room_2', 'desk'})
        self.move('kek', 'bookcase_0')
        self.assertSetEqual(self.get_ls_output(), {'bookcase_0', 'bookcase_1', 'bookcase_2', 'bookcase_3', 'room_2', 'desk'})

    def test_bookcase_move_outside_forbidden(self):
        self.move('bookcase_0', 'room_1/')
        self.assertSetEqual(self.get_ls_output(), {'bookcase_0', 'bookcase_1', 'bookcase_2', 'bookcase_3', 'room_1', 'desk', 'room_9'})
        self.move('bookcase_0', 'bookcase_1/')
        self.assertSetEqual(self.get_ls_output(), {'bookcase_0', 'bookcase_1', 'bookcase_2', 'bookcase_3', 'room_1', 'desk', 'room_9'})

    def test_shelf_renaming_ok(self):
        os.chdir('bookcase_3')
        self.move('shelf_3', 'kok')
        self.assertSetEqual(self.get_ls_output(), {'shelf_0', 'shelf_1', 'shelf_2', 'kok', 'shelf_4'})
        self.move('kok', 'shelf_3')
        self.assertSetEqual(self.get_ls_output(), {'shelf_0', 'shelf_1', 'shelf_2', 'shelf_3', 'shelf_4'})

    def test_shelf_move_outside_forbidden(self):
        os.chdir('bookcase_0')
        self.move('shelf_0', '..')
        self.assertSetEqual(self.get_ls_output(), {'shelf_0', 'shelf_1', 'shelf_2', 'shelf_3', 'shelf_4'})
        self.move('shelf_0', 'shelf_1')
        self.assertSetEqual(self.get_ls_output(), {'shelf_0', 'shelf_1', 'shelf_2', 'shelf_3', 'shelf_4'})
        self.move('shelf_0', '../room_1/bookcase_0/')
        self.assertSetEqual(self.get_ls_output(), {'shelf_0', 'shelf_1', 'shelf_2', 'shelf_3', 'shelf_4'})

    def test_desk_baskets_creation_ok(self):
        os.chdir('desk')
        os.mkdir('kok')
        self.assertSetEqual(self.get_ls_output(), {'kok'})
        os.chdir('kok')
        with open('ffff', 'w') as f:
            print('azaza', file=f)
        os.chdir('..')
        self.assertEqual(self.get_contents('kok/ffff'), 'azaza\n')

    def test_desk_nested_baskets_forbidden(self):
        os.chdir('room_9')
        self.assertSetEqual(self.get_ls_output(), {'desk', 'room_8', 'bookcase_0', 'bookcase_1', 'bookcase_2', 'bookcase_3'})
        os.chdir('desk')
        self.assertSetEqual(self.get_ls_output(), set())
        os.mkdir('kok12')
        self.assertSetEqual(self.get_ls_output(), {'kok12'})
        os.chdir('kok12')
        try:
            os.mkdir('kok256')
        except OSError as e:
            pass
        else:
            self.assertEqual('kek', 'kok')
        self.assertSetEqual(self.get_ls_output(), set())

    def test_book_renaming_forbidden(self):
        os.chdir('bookcase_0/shelf_0')
        ls_before = self.get_ls_output()
        self.move('book_0', 'book')
        ls_after = self.get_ls_output()
        self.assertEqual(ls_before, ls_after)
        
    def test_book_moving_to_desk_only(self):
        shelf = 'bookcase_0/shelf_0'
        ls_before = self.get_ls_output(shelf)
        self.move(f'{shelf}/book_0', 'bookcase_0')
        ls_after = self.get_ls_output(shelf)
        self.move(f'{shelf}/book_0', 'desk')
        self.assertSetEqual(ls_before, ls_after)
        self.assertSetEqual(self.get_ls_output('desk'), {'book_0'})
        self.move('desk/book_0', shelf)

if __name__ == '__main__':
    current_directory = os.getcwd()
    unittest.main()
